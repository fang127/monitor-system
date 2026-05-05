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
	Port          string
	Mode          string
	Version       string
	ManagerAddr   string
	ManagerTimeout time.Duration
}

func Load() Config {
	return Config{
		Port:           getEnv("API_GATEWAY_PORT", defaultPort),
		Mode:           getEnv("GIN_MODE", defaultMode),
		Version:        getEnv("API_GATEWAY_VERSION", defaultVersion),
		ManagerAddr:    getEnv("MANAGER_GRPC_ADDR", defaultManagerAddr),
		ManagerTimeout: getEnvDuration("MANAGER_GRPC_TIMEOUT", defaultGRPCTimeout),
	}
}

func getEnv(key string, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

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
