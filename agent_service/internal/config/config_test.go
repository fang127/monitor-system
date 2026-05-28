package config

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestConfigStringEnvOverridesYAML(t *testing.T) {
	t.Setenv("AGENT_CONFIG_PATH", writeConfig(t, "api_gateway_base_url: http://from-yaml\n"))
	t.Setenv("API_GATEWAY_BASE_URL", "http://from-env")
	resetForTest()

	got, err := ConfigString(context.Background(), "api_gateway_base_url", "API_GATEWAY_BASE_URL", "http://fallback")
	if err != nil {
		t.Fatalf("读取字符串配置失败: %v", err)
	}
	if got != "http://from-env" {
		t.Fatalf("环境变量应优先于 YAML，got=%q", got)
	}
}

func TestConfigStringFallsBackWhenConfigMissing(t *testing.T) {
	t.Setenv("AGENT_CONFIG_PATH", filepath.Join(t.TempDir(), "missing.yaml"))
	resetForTest()

	got, err := ConfigString(context.Background(), "missing_key", "MISSING_ENV", "fallback")
	if err != nil {
		t.Fatalf("有默认值时不应返回错误: %v", err)
	}
	if got != "fallback" {
		t.Fatalf("应返回默认值，got=%q", got)
	}
}

func TestConfigIntRejectsInvalidEnv(t *testing.T) {
	t.Setenv("AGENT_SERVICE_PORT", "bad-port")
	resetForTest()

	_, err := ConfigInt(context.Background(), "agent_service_port", "AGENT_SERVICE_PORT", 6872)
	if err == nil {
		t.Fatal("非法端口应返回错误")
	}
}

func writeConfig(t *testing.T, content string) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "config.yaml")
	if err := os.WriteFile(path, []byte(content), 0o600); err != nil {
		t.Fatalf("写入测试配置失败: %v", err)
	}
	return path
}
