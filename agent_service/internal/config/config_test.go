package config

import (
	"os"
	"path/filepath"
	"testing"
)

func TestLoadUsesDefaultsAndEnvOverrides(t *testing.T) {
	t.Setenv("AGENT_SERVICE_PORT", "7001")
	t.Setenv("API_GATEWAY_BASE_URL", "http://gateway:8080")
	t.Setenv("AGENT_MEMORY_ENABLED", "false")

	cfg, err := Load("")
	if err != nil {
		t.Fatalf("Load returned error: %v", err)
	}

	if cfg.Port != 7001 {
		t.Fatalf("Port = %d, want 7001", cfg.Port)
	}
	if cfg.APIGatewayBaseURL != "http://gateway:8080" {
		t.Fatalf("APIGatewayBaseURL = %q", cfg.APIGatewayBaseURL)
	}
	if cfg.MemoryEnabled {
		t.Fatal("MemoryEnabled = true, want false")
	}
	if cfg.DocsDir == "" || cfg.MemoryDir == "" {
		t.Fatalf("expected default docs and memory dirs, got docs=%q memory=%q", cfg.DocsDir, cfg.MemoryDir)
	}
}

func TestLoadReadsSimpleYAMLFallback(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "config.yaml")
	content := []byte("agent_service_port: 7010\napi_gateway_base_url: \"http://yaml:8080\"\ndocs_dir: ./yaml-docs\n")
	if err := os.WriteFile(path, content, 0644); err != nil {
		t.Fatal(err)
	}

	cfg, err := Load(path)
	if err != nil {
		t.Fatalf("Load returned error: %v", err)
	}

	if cfg.Port != 7010 {
		t.Fatalf("Port = %d, want 7010", cfg.Port)
	}
	if cfg.APIGatewayBaseURL != "http://yaml:8080" {
		t.Fatalf("APIGatewayBaseURL = %q", cfg.APIGatewayBaseURL)
	}
	if cfg.DocsDir != "./yaml-docs" {
		t.Fatalf("DocsDir = %q", cfg.DocsDir)
	}
}

func TestLoadRejectsInvalidPort(t *testing.T) {
	t.Setenv("AGENT_SERVICE_PORT", "not-a-number")
	_, err := Load("")
	if err == nil {
		t.Fatal("expected invalid port error")
	}
}
