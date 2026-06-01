package config

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync"

	"gopkg.in/yaml.v3"
)

// 该文件包含配置加载和环境变量读取函数，供整个 agent_service 使用。

var (
	configOnce sync.Once
	configData map[string]interface{}
	configErr  error
)

// ConfigString 从环境变量或配置文件中获取字符串类型的配置值，优先级为：环境变量 > 配置文件 > 默认值
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

// ConfigInt 从环境变量或配置文件中获取整数类型的配置值，优先级为：环境变量 > 配置文件 > 默认值
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

// ConfigFloat 从环境变量或配置文件中获取浮点类型的配置值，优先级为：环境变量 > 配置文件 > 默认值
func ConfigFloat(ctx context.Context, key string, envName string, fallback float64) (float64, error) {
	if value := os.Getenv(envName); value != "" {
		return strconv.ParseFloat(value, 64)
	}
	value, err := configValue(ctx, key)
	if err == nil && value != nil {
		switch typed := value.(type) {
		case int:
			return float64(typed), nil
		case int64:
			return float64(typed), nil
		case float64:
			return typed, nil
		case string:
			return strconv.ParseFloat(typed, 64)
		default:
			return strconv.ParseFloat(fmt.Sprintf("%v", value), 64)
		}
	}
	return fallback, nil
}

// ConfigBool 从环境变量或配置文件中获取布尔类型的配置值，优先级为：环境变量 > 配置文件 > 默认值
func ConfigBool(ctx context.Context, key string, envName string, fallback bool) (bool, error) {
	if value := os.Getenv(envName); value != "" {
		return strconv.ParseBool(value)
	}
	value, err := configValue(ctx, key)
	if err == nil && value != nil {
		switch typed := value.(type) {
		case bool:
			return typed, nil
		case string:
			return strconv.ParseBool(typed)
		default:
			return strconv.ParseBool(fmt.Sprintf("%v", value))
		}
	}
	return fallback, nil
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
	// 从根节点开始遍历配置数据
	// 通过点分隔的键路径逐层访问配置数据，例如 "database.host" 会依次访问 configData["database"]["host"]
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

// loadConfig 加载配置文件，优先级为：环境变量指定路径 > 默认路径1 > 默认路径2
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

// configPaths 返回一个包含可能的配置文件路径的切片，优先级为：环境变量指定路径 > 默认路径1 > 默认路径2
func configPaths() []string {
	if path := os.Getenv("AGENT_CONFIG_PATH"); path != "" {
		return []string{path, "manifest/config/config.yaml", "agent_service/manifest/config/config.yaml"}
	}
	return []string{"manifest/config/config.yaml", "agent_service/manifest/config/config.yaml"}
}

func resetForTest() {
	configOnce = sync.Once{}
	configData = nil
	configErr = nil
}
