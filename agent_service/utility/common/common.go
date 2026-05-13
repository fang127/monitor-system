package common

import (
	"context"
	"os"
	"strconv"

	"github.com/gogf/gf/v2/frame/g"
)

const (
	MilvusDBName         = "monitor_system_agent"
	MilvusCollectionName = "ops_docs"
)

var FileDir = "./docs/"

func ConfigString(ctx context.Context, key string, envName string, fallback string) (string, error) {
	if value := os.Getenv(envName); value != "" {
		return value, nil
	}
	value, err := g.Cfg().Get(ctx, key)
	if err == nil && !value.IsEmpty() {
		return value.String(), nil
	}
	if fallback != "" {
		return fallback, nil
	}
	return "", err
}

func ConfigInt(ctx context.Context, key string, envName string, fallback int) (int, error) {
	if value := os.Getenv(envName); value != "" {
		parsed, err := strconv.Atoi(value)
		if err != nil {
			return 0, err
		}
		return parsed, nil
	}
	value, err := g.Cfg().Get(ctx, key)
	if err == nil && !value.IsEmpty() {
		return value.Int(), nil
	}
	if fallback != 0 {
		return fallback, nil
	}
	return fallback, err
}
