package config

import (
	"fmt"
	"os"
	"strconv"
	"strings"
)

type ModelConfig struct {
	APIKey  string
	BaseURL string
	Model   string
}

type Config struct {
	Port              int
	APIGatewayBaseURL string
	MilvusAddr        string
	DocsDir           string
	MemoryDir         string
	MemoryEnabled     bool
	ThinkModel        ModelConfig
	QuickModel        ModelConfig
	EmbeddingAPIKey   string
	EmbeddingModel    string
	EmbeddingDim      int
}

func Load(path string) (Config, error) {
	values := map[string]string{}
	if path != "" {
		loaded, err := readSimpleYAML(path)
		if err != nil {
			return Config{}, err
		}
		values = loaded
	} else {
		for _, candidate := range []string{"manifest/config/config.yaml", "agent_service/manifest/config/config.yaml"} {
			loaded, err := readSimpleYAML(candidate)
			if err == nil {
				values = loaded
				break
			}
		}
	}

	port, err := intValue("AGENT_SERVICE_PORT", values["agent_service_port"], 6872)
	if err != nil {
		return Config{}, fmt.Errorf("invalid AGENT_SERVICE_PORT: %w", err)
	}

	embeddingDim, err := intValue("AGENT_EMBEDDING_DIMENSIONS", values["doubao_embedding_model.dimensions"], 2048)
	if err != nil {
		return Config{}, fmt.Errorf("invalid AGENT_EMBEDDING_DIMENSIONS: %w", err)
	}

	return Config{
		Port:              port,
		APIGatewayBaseURL: stringValue("API_GATEWAY_BASE_URL", values["api_gateway_base_url"], "http://127.0.0.1:8080"),
		MilvusAddr:        stringValue("MILVUS_ADDR", values["milvus_addr"], "127.0.0.1:19530"),
		DocsDir:           stringValue("AGENT_DOCS_DIR", values["docs_dir"], "./docs"),
		MemoryDir:         stringValue("AGENT_MEMORY_DIR", values["memory_dir"], "./memory"),
		MemoryEnabled:     boolValue("AGENT_MEMORY_ENABLED", values["memory_enabled"], true),
		ThinkModel: ModelConfig{
			APIKey:  stringValue("AGENT_THINK_API_KEY", values["ds_think_chat_model.api_key"], ""),
			BaseURL: stringValue("AGENT_THINK_BASE_URL", values["ds_think_chat_model.base_url"], ""),
			Model:   stringValue("AGENT_THINK_MODEL", values["ds_think_chat_model.model"], ""),
		},
		QuickModel: ModelConfig{
			APIKey:  stringValue("AGENT_QUICK_API_KEY", values["ds_quick_chat_model.api_key"], ""),
			BaseURL: stringValue("AGENT_QUICK_BASE_URL", values["ds_quick_chat_model.base_url"], ""),
			Model:   stringValue("AGENT_QUICK_MODEL", values["ds_quick_chat_model.model"], ""),
		},
		EmbeddingAPIKey: stringValue("AGENT_EMBEDDING_API_KEY", values["doubao_embedding_model.api_key"], ""),
		EmbeddingModel:  stringValue("AGENT_EMBEDDING_MODEL", values["doubao_embedding_model.model"], "text-embedding-v4"),
		EmbeddingDim:    embeddingDim,
	}, nil
}

func stringValue(envName, configured, fallback string) string {
	if value := strings.TrimSpace(os.Getenv(envName)); value != "" {
		return value
	}
	if strings.TrimSpace(configured) != "" {
		return strings.TrimSpace(configured)
	}
	return fallback
}

func intValue(envName, configured string, fallback int) (int, error) {
	raw := stringValue(envName, configured, "")
	if raw == "" {
		return fallback, nil
	}
	return strconv.Atoi(raw)
}

func boolValue(envName, configured string, fallback bool) bool {
	raw := stringValue(envName, configured, "")
	if raw == "" {
		return fallback
	}
	parsed, err := strconv.ParseBool(raw)
	if err != nil {
		return fallback
	}
	return parsed
}

func readSimpleYAML(path string) (map[string]string, error) {
	content, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	values := map[string]string{}
	var prefix []string
	for _, line := range strings.Split(string(content), "\n") {
		if idx := strings.Index(line, "#"); idx >= 0 {
			line = line[:idx]
		}
		if strings.TrimSpace(line) == "" {
			continue
		}
		indent := len(line) - len(strings.TrimLeft(line, " "))
		line = strings.TrimSpace(line)
		parts := strings.SplitN(line, ":", 2)
		key := strings.TrimSpace(parts[0])
		if key == "" {
			continue
		}
		level := indent / 2
		if level < len(prefix) {
			prefix = prefix[:level]
		}
		if len(parts) == 1 || strings.TrimSpace(parts[1]) == "" {
			prefix = append(prefix, key)
			continue
		}
		full := append(append([]string{}, prefix...), key)
		values[strings.Join(full, ".")] = cleanYAMLScalar(parts[1])
	}
	return values, nil
}

func cleanYAMLScalar(raw string) string {
	value := strings.TrimSpace(raw)
	value = strings.Trim(value, `"'`)
	return value
}
