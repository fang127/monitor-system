package config

import (
	"os"
	"time"
)

const (
	defaultPort         = "8080"
	defaultMode         = "debug"
	defaultVersion      = "v0.1.0"
	defaultManagerAddr  = "127.0.0.1:50051"
	defaultGRPCTimeout  = 5 * time.Second
	defaultJWTSecret    = "monitor-system-dev-secret"
	defaultJWTAccessTTL = 24 * time.Hour
)

type Config struct {
	Port           string
	Mode           string
	Version        string
	ManagerAddr    string
	ManagerTimeout time.Duration
	JWTSecret      string
	JWTAccessTTL   time.Duration
	MySQLHost      string
	MySQLPort      string
	MySQLUser      string
	MySQLPassword  string
	MySQLDatabase  string
	AdminUsername  string
	AdminPassword  string
}

// 加载配置，从环境变量获取，如果没有设置则使用默认值
func Load() Config {
	return Config{
		Port:           getEnv("API_GATEWAY_PORT", defaultPort),
		Mode:           getEnv("GIN_MODE", defaultMode),
		Version:        getEnv("API_GATEWAY_VERSION", defaultVersion),
		ManagerAddr:    getEnv("MANAGER_GRPC_ADDR", defaultManagerAddr),
		ManagerTimeout: getEnvDuration("MANAGER_GRPC_TIMEOUT", defaultGRPCTimeout),
		JWTSecret:      getEnv("JWT_SECRET", defaultJWTSecret),
		JWTAccessTTL:   getEnvDuration("JWT_ACCESS_TTL", defaultJWTAccessTTL),
		MySQLHost:      getEnv("MYSQL_HOST", "127.0.0.1"),
		MySQLPort:      getEnv("MYSQL_PORT", "3306"),
		MySQLUser:      getEnv("MYSQL_USER", "root"),
		MySQLPassword:  getEnv("MYSQL_PASSWORD", ""),
		MySQLDatabase:  getEnv("MYSQL_DATABASE", "monitor-system"),
		AdminUsername:  getEnv("ADMIN_USERNAME", ""),
		AdminPassword:  getEnv("ADMIN_PASSWORD", ""),
	}
}

func (c Config) MySQLDSN() string {
	return c.MySQLUser + ":" + c.MySQLPassword + "@tcp(" + c.MySQLHost + ":" + c.MySQLPort + ")/" + c.MySQLDatabase + "?parseTime=true&charset=utf8mb4"
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
