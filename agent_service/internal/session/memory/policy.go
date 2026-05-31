package memory

import "context"

// ResolvePolicy 按全局、租户、团队、集群顺序合并策略，越具体的作用域优先级越高。
func ResolvePolicy(ctx context.Context, store MemoryStore, cfg MemoryConfig, scope MemoryScope) (EffectivePolicy, error) {
	effective := EffectivePolicy{
		LongTermEnabled: cfg.LongTermEnabled,
		WriteEnabled:    cfg.WriteEnabled,
		RecallEnabled:   cfg.RecallEnabled,
		RecentWindow:    positiveOrDefault(cfg.RecentWindow, DefaultConfig().RecentWindow),
		SummaryWindow:   positiveOrDefault(cfg.SummaryWindow, DefaultConfig().SummaryWindow),
		RecallLimit:     positiveOrDefault(cfg.RecallLimit, DefaultConfig().RecallLimit),
		TokenBudget:     positiveOrDefault(cfg.TokenBudget, DefaultConfig().TokenBudget),
	}

	scope = scope.Normalized()
	levels := []ScopeLevel{ScopeLevelGlobal}
	if scope.TenantID != "" {
		levels = append(levels, ScopeLevelTenant)
	}
	if scope.TenantID != "" && scope.TeamID != "" {
		levels = append(levels, ScopeLevelTeam)
	}
	if scope.TenantID != "" && scope.TeamID != "" && scope.ClusterID != "" {
		levels = append(levels, ScopeLevelCluster)
	}
	for _, level := range levels {
		policy, err := store.GetPolicy(ctx, level, scope)
		if err != nil {
			return effective, err
		}
		if policy == nil {
			continue
		}
		applyPolicy(&effective, *policy)
	}
	return effective, nil
}

func applyPolicy(effective *EffectivePolicy, policy MemoryPolicy) {
	if policy.LongTermEnabled != nil {
		effective.LongTermEnabled = *policy.LongTermEnabled
	}
	if policy.WriteEnabled != nil {
		effective.WriteEnabled = *policy.WriteEnabled
	}
	if policy.RecallEnabled != nil {
		effective.RecallEnabled = *policy.RecallEnabled
	}
	if policy.RecentWindow > 0 {
		effective.RecentWindow = policy.RecentWindow
	}
	if policy.SummaryWindow > 0 {
		effective.SummaryWindow = policy.SummaryWindow
	}
	if policy.RecallLimit > 0 {
		effective.RecallLimit = policy.RecallLimit
	}
	if policy.TokenBudget > 0 {
		effective.TokenBudget = policy.TokenBudget
	}
}

func positiveOrDefault(value int, fallback int) int {
	if value > 0 {
		return value
	}
	return fallback
}

func boolPtr(value bool) *bool {
	return &value
}
