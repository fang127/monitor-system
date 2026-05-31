package memory

import (
	"context"
	"testing"
)

func TestResolvePolicyUsesMoreSpecificScope(t *testing.T) {
	ctx := context.Background()
	store := NewInMemoryStore()
	scope := MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a", SessionID: "s1"}

	if err := store.SetPolicy(ctx, ScopeLevelGlobal, scope, MemoryPolicy{LongTermEnabled: boolPtr(true), RecallLimit: 3}); err != nil {
		t.Fatalf("设置全局策略失败: %v", err)
	}
	if err := store.SetPolicy(ctx, ScopeLevelTeam, scope, MemoryPolicy{RecallEnabled: boolPtr(true), RecallLimit: 7}); err != nil {
		t.Fatalf("设置团队策略失败: %v", err)
	}
	if err := store.SetPolicy(ctx, ScopeLevelCluster, scope, MemoryPolicy{WriteEnabled: boolPtr(false), RecentWindow: 10}); err != nil {
		t.Fatalf("设置集群策略失败: %v", err)
	}

	policy, err := ResolvePolicy(ctx, store, DefaultConfig(), scope)
	if err != nil {
		t.Fatalf("合并策略失败: %v", err)
	}
	if !policy.LongTermEnabled {
		t.Fatal("应继承全局长期记忆开关")
	}
	if !policy.RecallEnabled {
		t.Fatal("应继承团队召回开关")
	}
	if policy.WriteEnabled {
		t.Fatal("集群写入开关应覆盖为关闭")
	}
	if policy.RecallLimit != 7 {
		t.Fatalf("团队召回数量应覆盖全局配置，got=%d", policy.RecallLimit)
	}
	if policy.RecentWindow != 10 {
		t.Fatalf("集群最近消息窗口应生效，got=%d", policy.RecentWindow)
	}
}

func TestMemoryScopeNormalizedOnlyTrimsFields(t *testing.T) {
	scope := MemoryScope{
		TenantID:  "  ",
		TeamID:    " team-a ",
		ClusterID: " cluster-a ",
		SessionID: " demo ",
	}.Normalized()
	if scope.TenantID != "" || scope.TeamID != "team-a" || scope.ClusterID != "cluster-a" || scope.SessionID != "demo" {
		t.Fatalf("作用域规范化结果不正确: %+v", scope)
	}
}

func TestResolvePolicySkipsMissingScopeLevels(t *testing.T) {
	ctx := context.Background()
	store := NewInMemoryStore()
	scope := MemoryScope{TenantID: "tenant-a"}

	if err := store.SetPolicy(ctx, ScopeLevelGlobal, scope, MemoryPolicy{RecallLimit: 3}); err != nil {
		t.Fatalf("设置全局策略失败: %v", err)
	}
	if err := store.SetPolicy(ctx, ScopeLevelTeam, MemoryScope{TenantID: "tenant-a", TeamID: "default"}, MemoryPolicy{RecallLimit: 9}); err != nil {
		t.Fatalf("设置团队策略失败: %v", err)
	}

	policy, err := ResolvePolicy(ctx, store, DefaultConfig(), scope)
	if err != nil {
		t.Fatalf("合并策略失败: %v", err)
	}
	if policy.RecallLimit != 3 {
		t.Fatalf("缺少团队时不应命中默认团队策略，got=%d", policy.RecallLimit)
	}
}
