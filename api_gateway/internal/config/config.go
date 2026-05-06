package config

import (
	"os"
	"time"
)

const (
	defaultPort        = "8080"
	defaultMode        = "debug"
	defaultVersion     = "v0.1.0"
	defaultManagerAddr = "127.0.0.1:50051"
	defaultGRPCTimeout = 5 * time.Second
)

type Config struct {
	Port           string
	Mode           string
	Version        string
	ManagerAddr    string
	ManagerTimeout time.Duration
}

// 加载配置，从环境变量获取，如果没有设置则使用默认值
func Load() Config {
	return Config{
		Port:           getEnv("API_GATEWAY_PORT", defaultPort),
		Mode:           getEnv("GIN_MODE", defaultMode),
		Version:        getEnv("API_GATEWAY_VERSION", defaultVersion),
		ManagerAddr:    getEnv("MANAGER_GRPC_ADDR", defaultManagerAddr),
		ManagerTimeout: getEnvDuration("MANAGER_GRPC_TIMEOUT", defaultGRPCTimeout),
	}
}

// 从环境变量获取值，如果没有设置则返回默认值
func getEnv(key string, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

// 从环境变量获取时间值，如果没有设置或解析失败则返回默认值
func getEnvDuration(key string, fallback time.Duration) time.Duration {
	value := os.Getenv(key)
	if value == "" {
		return fallback
	}
	duration, err := time.ParseDuration(value)
	if err != nil {
		return fallback
	}
	return duration
}
