package common

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync"

	"gopkg.in/yaml.v3"
)

const (
	MilvusDBName         = "monitor_system_agent"
	MilvusCollectionName = "ops_docs"
)

var FileDir = "./docs/"

var (
	configOnce sync.Once
	configData map[string]interface{}
	configErr  error
)

func ConfigString(ctx context.Context, key string, envName string, fallback string) (string, error) {
	if value := os.Getenv(envName); value != "" {
		return value, nil
	}
	value, err := configValue(ctx, key)
	if err == nil && value != nil {
		return fmt.Sprintf("%v", value), nil
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
	value, err := configValue(ctx, key)
	if err == nil && value != nil {
		switch typed := value.(type) {
		case int:
			return typed, nil
		case int64:
			return int(typed), nil
		case float64:
			return int(typed), nil
		case string:
			return strconv.Atoi(typed)
		default:
			return strconv.Atoi(fmt.Sprintf("%v", value))
		}
	}
	if fallback != 0 {
		return fallback, nil
	}
	return fallback, err
}

func configValue(ctx context.Context, key string) (interface{}, error) {
	_ = ctx
	configOnce.Do(loadConfig)
	if configErr != nil && len(configData) == 0 {
		return nil, configErr
	}
	if len(configData) == 0 {
		return nil, fmt.Errorf("config file not found")
	}
	current := interface{}(configData)
	for _, part := range strings.Split(key, ".") {
		switch node := current.(type) {
		case map[string]interface{}:
			next, ok := node[part]
			if !ok {
				return nil, fmt.Errorf("config key %q not found", key)
			}
			current = next
		default:
			return nil, fmt.Errorf("config key %q not found", key)
		}
	}
	return current, nil
}

func loadConfig() {
	configData = map[string]interface{}{}
	for _, path := range configPaths() {
		content, err := os.ReadFile(path)
		if err != nil {
			configErr = err
			continue
		}
		if err := yaml.Unmarshal(content, &configData); err != nil {
			configErr = err
			return
		}
		configErr = nil
		return
	}
}

func configPaths() []string {
	if path := os.Getenv("AGENT_CONFIG_PATH"); path != "" {
		return []string{path, "manifest/config/config.yaml", "agent_service/manifest/config/config.yaml"}
	}
	return []string{"manifest/config/config.yaml", "agent_service/manifest/config/config.yaml"}
}
