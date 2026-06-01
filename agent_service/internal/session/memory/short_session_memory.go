// 该文件实现了 RedisSessionStore，使用 Redis 作为会话热存储的后端。它提供 LoadSession、SaveSession 和 DeleteSession 方法，先访问本地 LRU 缓存，再访问 Redis，并在 Redis 不可用时降级到本地缓存。Redis 会话数据以 JSON 格式存储，键名基于 MemoryScope 生成。
package memory

import (
	"context"
	"encoding/json"
	"fmt"
	"time"
)

// RedisKVClient 是 Redis 会话热存储需要的最小客户端接口，便于接入 go-redis 或测试替身。
type RedisKVClient interface {
	Get(ctx context.Context, key string) (string, error)
	Set(ctx context.Context, key string, value string, ttl time.Duration) error
	Del(ctx context.Context, key string) error
}

// RedisSessionStore 使用 Redis 保存短期会话热状态。它不写 MySQL recent messages。
type ShortSessionStore struct {
	client RedisKVClient
	ttl    time.Duration
	local  *LRUSessionStore
}

// NewRedisSessionStore 创建 Redis 短期会话热存储，local 用作一级缓存和 Redis 失败降级。
func NewRedisSessionStore(client RedisKVClient, cfg MemoryConfig, local *LRUSessionStore) *ShortSessionStore {
	cfg = normalizeConfig(cfg)
	if local == nil {
		local = NewLRUSessionStore(cfg)
	}
	return &ShortSessionStore{client: client, ttl: time.Duration(cfg.SessionTTLSeconds) * time.Second, local: local}
}

// LoadSession 先读本地 LRU，再读 Redis，Redis 未配置时直接使用本地降级。
func (s *ShortSessionStore) LoadSession(ctx context.Context, scope MemoryScope) (*SessionMemory, error) {
	if s == nil || s.local == nil {
		return nil, fmt.Errorf("Redis 会话存储未初始化")
	}
	localSession, err := s.local.LoadSession(ctx, scope)
	if err != nil {
		return nil, err
	}
	if len(localSession.Messages) > 0 || localSession.Summary != "" || s.client == nil {
		return localSession, nil
	}
	payload, err := s.client.Get(ctx, redisSessionKey(scope))
	if err != nil || payload == "" {
		return localSession, nil
	}
	var session SessionMemory
	if err := json.Unmarshal([]byte(payload), &session); err != nil {
		return localSession, nil
	}
	if err := s.local.SaveSession(ctx, &session); err != nil {
		return nil, err
	}
	return &session, nil
}

// SaveSession 写入本地 LRU，并尽力写入 Redis。
func (s *ShortSessionStore) SaveSession(ctx context.Context, session *SessionMemory) error {
	if s == nil || s.local == nil {
		return fmt.Errorf("Redis 会话存储未初始化")
	}
	if err := s.local.SaveSession(ctx, session); err != nil {
		return err
	}
	if s.client == nil || session == nil {
		return nil
	}
	payload, err := json.Marshal(session)
	if err != nil {
		return err
	}
	// Redis 写入失败不返回错误，继续使用本地缓存。
	// TODO：可以引入RabbitMQ等消息队列异步重试写入Redis，避免同步调用Redis导致的性能问题。
	return s.client.Set(ctx, redisSessionKey(session.Scope), string(payload), s.ttl)
}

// DeleteSession 删除本地和 Redis 短期会话。
func (s *ShortSessionStore) DeleteSession(ctx context.Context, scope MemoryScope) error {
	if s == nil || s.local == nil {
		return fmt.Errorf("Redis 会话存储未初始化")
	}
	if err := s.local.DeleteSession(ctx, scope); err != nil {
		return err
	}
	if s.client == nil {
		return nil
	}
	return s.client.Del(ctx, redisSessionKey(scope))
}

func redisSessionKey(scope MemoryScope) string {
	return "agent:session:" + scope.Normalized().SessionKey()
}
