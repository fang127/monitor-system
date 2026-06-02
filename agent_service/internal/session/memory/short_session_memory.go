// 该文件实现短期会话 Redis 存储。短期记忆只保存在 Redis 本地服务器内存中，不写入 MySQL。
package memory

import (
	"context"
	"encoding/json"
	"fmt"
	"time"
)

// RedisKVClient 是 Redis 会话存储需要的最小客户端接口，便于接入 go-redis 或测试替身。
type RedisKVClient interface {
	Get(ctx context.Context, key string) (string, error)
	Set(ctx context.Context, key string, value string, ttl time.Duration) error
	Del(ctx context.Context, key string) error
}

// ShortSessionStore 使用 Redis 保存短期会话状态，并保留同进程 LRU 作为运行期兜底。
// Redis 仍是启动强依赖；local 只用于减少热读压力和 Redis 短暂异常时保住当前会话。
type ShortSessionStore struct {
	client RedisKVClient
	ttl    time.Duration
	local  *LRUSessionStore
}

// NewRedisSessionStore 创建 Redis 短期会话存储。
func NewRedisSessionStore(client RedisKVClient, cfg MemoryConfig, local *LRUSessionStore) *ShortSessionStore {
	cfg = normalizeConfig(cfg)
	if local == nil {
		local = NewLRUSessionStore(cfg)
	}
	return &ShortSessionStore{client: client, ttl: time.Duration(cfg.SessionTTLSeconds) * time.Second, local: local}
}

// LoadSession 优先读取本地热会话，未命中时读取 Redis；Redis 读取失败时使用本地空会话兜底。
func (s *ShortSessionStore) LoadSession(ctx context.Context, scope MemoryScope) (*SessionMemory, error) {
	if s == nil || s.client == nil {
		return nil, fmt.Errorf("Redis 会话存储未初始化")
	}
	scope = scope.Normalized()
	if err := ValidateSessionScope(scope); err != nil {
		return nil, err
	}
	if s.local != nil {
		localSession, err := s.local.LoadSession(ctx, scope)
		if err != nil {
			return nil, err
		}
		if localSession != nil && (len(localSession.Messages) > 0 || localSession.Summary != "") {
			return localSession, nil
		}
	}
	payload, err := s.client.Get(ctx, redisSessionKey(scope))
	if err != nil {
		if s.local != nil {
			localSession, localErr := s.local.LoadSession(ctx, scope)
			if localErr != nil {
				return nil, localErr
			}
			return localSession, nil
		}
		return &SessionMemory{Scope: scope, Messages: nil, UpdatedAt: time.Now()}, nil
	}
	if payload == "" {
		return &SessionMemory{Scope: scope, Messages: nil, UpdatedAt: time.Now()}, nil
	}
	var session SessionMemory
	if err := json.Unmarshal([]byte(payload), &session); err != nil {
		return nil, err
	}
	session.Scope = session.Scope.Normalized()
	if s.local != nil {
		if err := s.local.SaveSession(ctx, &session); err != nil {
			return nil, err
		}
	}
	return &session, nil
}

// SaveSession 先写本地 LRU，再写 Redis；Redis 失败会返回错误，调用方负责记录。
func (s *ShortSessionStore) SaveSession(ctx context.Context, session *SessionMemory) error {
	if s == nil || s.client == nil {
		return fmt.Errorf("Redis 会话存储未初始化")
	}
	if session == nil {
		return nil
	}
	session.Scope = session.Scope.Normalized()
	if err := ValidateSessionScope(session.Scope); err != nil {
		return err
	}
	if s.local != nil {
		if err := s.local.SaveSession(ctx, session); err != nil {
			return err
		}
	}
	payload, err := json.Marshal(session)
	if err != nil {
		return err
	}
	return s.client.Set(ctx, redisSessionKey(session.Scope), string(payload), s.ttl)
}

// DeleteSession 删除本地和 Redis 短期会话。
func (s *ShortSessionStore) DeleteSession(ctx context.Context, scope MemoryScope) error {
	if s == nil || s.client == nil {
		return fmt.Errorf("Redis 会话存储未初始化")
	}
	scope = scope.Normalized()
	if err := ValidateSessionScope(scope); err != nil {
		return err
	}
	if s.local != nil {
		if err := s.local.DeleteSession(ctx, scope); err != nil {
			return err
		}
	}
	return s.client.Del(ctx, redisSessionKey(scope))
}

func redisSessionKey(scope MemoryScope) string {
	return "agent:session:" + scope.Normalized().SessionKey()
}
