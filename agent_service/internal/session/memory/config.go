package memory

import (
	"context"
	"monitor-system/agent_service/internal/config"
)

// LoadConfig 从配置文件和环境变量读取记忆系统配置。
func LoadConfig(ctx context.Context) (MemoryConfig, error) {
	cfg := DefaultConfig()
	var err error
	if cfg.LongTermEnabled, err = config.ConfigBool(ctx, "memory.long_term_enabled", "AGENT_MEMORY_LONG_TERM_ENABLED", cfg.LongTermEnabled); err != nil {
		return cfg, err
	}
	if cfg.WriteEnabled, err = config.ConfigBool(ctx, "memory.write_enabled", "AGENT_MEMORY_WRITE_ENABLED", cfg.WriteEnabled); err != nil {
		return cfg, err
	}
	if cfg.RecallEnabled, err = config.ConfigBool(ctx, "memory.recall_enabled", "AGENT_MEMORY_RECALL_ENABLED", cfg.RecallEnabled); err != nil {
		return cfg, err
	}
	if cfg.RecentWindow, err = config.ConfigInt(ctx, "memory.recent_window", "AGENT_MEMORY_RECENT_WINDOW", cfg.RecentWindow); err != nil {
		return cfg, err
	}
	if cfg.SummaryWindow, err = config.ConfigInt(ctx, "memory.summary_window", "AGENT_MEMORY_SUMMARY_WINDOW", cfg.SummaryWindow); err != nil {
		return cfg, err
	}
	if cfg.RecallLimit, err = config.ConfigInt(ctx, "memory.recall_limit", "AGENT_MEMORY_RECALL_LIMIT", cfg.RecallLimit); err != nil {
		return cfg, err
	}
	if cfg.TokenBudget, err = config.ConfigInt(ctx, "memory.token_budget", "AGENT_MEMORY_TOKEN_BUDGET", cfg.TokenBudget); err != nil {
		return cfg, err
	}
	return cfg, nil
}
