package redis

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/config"
	"strings"
	"time"

	goredis "github.com/redis/go-redis/v9"
)

// Config 描述 agent_service 自己的 Redis 配置，不复用 manager 的 Redis 配置。
type Config struct {
	Enabled      bool
	Addr         string
	Password     string
	DB           int
	DialTimeout  time.Duration
	ReadTimeout  time.Duration
	WriteTimeout time.Duration
}

// Client 适配短期会话存储需要的 RedisKVClient 接口。
type Client struct {
	inner *goredis.Client
}

// LoadConfig 读取 agent_service 独立 Redis 配置。
func LoadConfig(ctx context.Context) (Config, error) {
	cfg := Config{Addr: "127.0.0.1:6379", DialTimeout: 500 * time.Millisecond, ReadTimeout: time.Second, WriteTimeout: time.Second}
	var err error
	if cfg.Enabled, err = config.ConfigBool(ctx, "agent_redis.enabled", "AGENT_REDIS_ENABLED", cfg.Enabled); err != nil {
		return cfg, err
	}
	if cfg.Addr, err = config.ConfigString(ctx, "agent_redis.addr", "AGENT_REDIS_ADDR", cfg.Addr); err != nil {
		return cfg, err
	}
	cfg.Addr = strings.TrimPrefix(strings.TrimPrefix(cfg.Addr, "tcp://"), "redis://")
	if cfg.Password, err = config.ConfigString(ctx, "agent_redis.password", "AGENT_REDIS_PASSWORD", cfg.Password); err != nil {
		return cfg, err
	}
	if cfg.DB, err = config.ConfigInt(ctx, "agent_redis.db", "AGENT_REDIS_DB", cfg.DB); err != nil {
		return cfg, err
	}
	dialMs, err := config.ConfigInt(ctx, "agent_redis.dial_timeout_ms", "AGENT_REDIS_DIAL_TIMEOUT_MS", int(cfg.DialTimeout.Milliseconds()))
	if err != nil {
		return cfg, err
	}
	readMs, err := config.ConfigInt(ctx, "agent_redis.read_timeout_ms", "AGENT_REDIS_READ_TIMEOUT_MS", int(cfg.ReadTimeout.Milliseconds()))
	if err != nil {
		return cfg, err
	}
	writeMs, err := config.ConfigInt(ctx, "agent_redis.write_timeout_ms", "AGENT_REDIS_WRITE_TIMEOUT_MS", int(cfg.WriteTimeout.Milliseconds()))
	if err != nil {
		return cfg, err
	}
	cfg.DialTimeout = time.Duration(dialMs) * time.Millisecond
	cfg.ReadTimeout = time.Duration(readMs) * time.Millisecond
	cfg.WriteTimeout = time.Duration(writeMs) * time.Millisecond
	return cfg, nil
}

// Open 创建 agent_service 自己的 Redis 客户端。
func Open(ctx context.Context) (*Client, Config, error) {
	cfg, err := LoadConfig(ctx)
	if err != nil {
		return nil, cfg, err
	}
	if !cfg.Enabled {
		return nil, cfg, nil
	}
	client := goredis.NewClient(&goredis.Options{Addr: cfg.Addr, Password: cfg.Password, DB: cfg.DB, DialTimeout: cfg.DialTimeout, ReadTimeout: cfg.ReadTimeout, WriteTimeout: cfg.WriteTimeout})
	if err := client.Ping(ctx).Err(); err != nil {
		_ = client.Close()
		return nil, cfg, fmt.Errorf("连接 agent_service Redis 失败: %w", err)
	}
	return &Client{inner: client}, cfg, nil
}

// Close 关闭 Redis 客户端。
func (c *Client) Close() error {
	if c == nil || c.inner == nil {
		return nil
	}
	return c.inner.Close()
}

// Get 读取字符串值。
func (c *Client) Get(ctx context.Context, key string) (string, error) {
	if c == nil || c.inner == nil {
		return "", fmt.Errorf("Redis 客户端未初始化")
	}
	value, err := c.inner.Get(ctx, key).Result()
	if err == goredis.Nil {
		return "", nil
	}
	return value, err
}

// Set 写入字符串值。
func (c *Client) Set(ctx context.Context, key string, value string, ttl time.Duration) error {
	if c == nil || c.inner == nil {
		return fmt.Errorf("Redis 客户端未初始化")
	}
	return c.inner.Set(ctx, key, value, ttl).Err()
}

// Del 删除键。
func (c *Client) Del(ctx context.Context, key string) error {
	if c == nil || c.inner == nil {
		return fmt.Errorf("Redis 客户端未初始化")
	}
	return c.inner.Del(ctx, key).Err()
}
