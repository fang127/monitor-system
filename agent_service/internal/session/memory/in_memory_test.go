package memory

import (
	"context"
	"strings"
	"testing"
)

func TestManagerKeepsRecentWindowAndSummary(t *testing.T) {
	ctx := context.Background()
	cfg := DefaultConfig()
	cfg.RecentWindow = 2
	cfg.SummaryWindow = 4
	manager := NewInMemoryManager(cfg)
	scope := MemoryScope{TenantID: "tenant-a", TeamID: "team-a", SessionID: "s1"}

	if err := manager.AppendTurn(ctx, scope, "第一轮问题", "第一轮回答"); err != nil {
		t.Fatalf("追加第一轮失败: %v", err)
	}
	if err := manager.AppendTurn(ctx, scope, "第二轮问题", "第二轮回答"); err != nil {
		t.Fatalf("追加第二轮失败: %v", err)
	}

	memCtx, err := manager.LoadContext(ctx, scope, "继续")
	if err != nil {
		t.Fatalf("加载记忆失败: %v", err)
	}
	if len(memCtx.RecentMessages) != 2 {
		t.Fatalf("最近消息窗口应只保留 2 条，got=%d", len(memCtx.RecentMessages))
	}
	if !strings.Contains(memCtx.Summary, "第二轮问题") {
		t.Fatalf("摘要应包含最近会话内容，got=%q", memCtx.Summary)
	}
}

func TestLongTermMemoryRequiresExplicitEnablement(t *testing.T) {
	ctx := context.Background()
	manager := NewInMemoryManager(DefaultConfig())
	scope := MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a", SessionID: "s1"}

	if err := manager.AppendTurn(ctx, scope, "请记住：这个集群默认优先看 Redis 慢日志", "好的"); err != nil {
		t.Fatalf("追加会话失败: %v", err)
	}
	if err := manager.ExtractDurableMemories(ctx, scope); err != nil {
		t.Fatalf("关闭长期记忆时抽取不应失败: %v", err)
	}
	items, err := manager.ListMemories(ctx, MemorySelector{Scope: scope, IncludeDeleted: true})
	if err != nil {
		t.Fatalf("查询长期记忆失败: %v", err)
	}
	if len(items) != 0 {
		t.Fatalf("长期记忆未显式启用时不应写入，got=%d", len(items))
	}

	if err := manager.SetLongTermEnabled(ctx, scope, true); err != nil {
		t.Fatalf("启用长期记忆失败: %v", err)
	}
	if err := manager.ExtractDurableMemories(ctx, scope); err != nil {
		t.Fatalf("启用长期记忆后抽取失败: %v", err)
	}
	items, err = manager.ListMemories(ctx, MemorySelector{Scope: scope})
	if err != nil {
		t.Fatalf("查询长期记忆失败: %v", err)
	}
	if len(items) != 1 {
		t.Fatalf("应写入一条长期记忆，got=%d", len(items))
	}
}

func TestDeleteMemoriesFiltersRecall(t *testing.T) {
	ctx := context.Background()
	cfg := DefaultConfig()
	cfg.LongTermEnabled = true
	cfg.RecallEnabled = true
	manager := NewInMemoryManager(cfg)
	scope := MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a", SessionID: "s1"}

	saved, err := manager.store.SaveLongTermMemory(ctx, LongTermMemory{
		Scope:      scope,
		Type:       "cluster_knowledge",
		Content:    "cluster-a 的 Redis 慢日志需要优先检查",
		Confidence: 0.9,
		Status:     LongTermStatusActive,
	})
	if err != nil {
		t.Fatalf("保存长期记忆失败: %v", err)
	}
	if err := manager.index.Upsert(ctx, saved); err != nil {
		t.Fatalf("写入索引失败: %v", err)
	}
	before, err := manager.LoadContext(ctx, scope, "Redis")
	if err != nil {
		t.Fatalf("删除前加载上下文失败: %v", err)
	}
	if len(before.LongTerm) != 1 {
		t.Fatalf("删除前应召回长期记忆，got=%d", len(before.LongTerm))
	}

	if err := manager.DeleteMemories(ctx, MemorySelector{ID: saved.ID, Scope: scope}); err != nil {
		t.Fatalf("删除长期记忆失败: %v", err)
	}
	after, err := manager.LoadContext(ctx, scope, "Redis")
	if err != nil {
		t.Fatalf("删除后加载上下文失败: %v", err)
	}
	if len(after.LongTerm) != 0 {
		t.Fatalf("删除后不应召回长期记忆，got=%d", len(after.LongTerm))
	}
}

func TestRetryDeletingMemoriesMarksDeleted(t *testing.T) {
	ctx := context.Background()
	manager := NewInMemoryManager(DefaultConfig())
	scope := MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a"}
	saved, err := manager.store.SaveLongTermMemory(ctx, LongTermMemory{
		Scope:      scope,
		Type:       "team_note",
		Content:    "请记住：团队默认中文输出",
		Confidence: 0.9,
		Status:     LongTermStatusDeleting,
	})
	if err != nil {
		t.Fatalf("保存 deleting 记忆失败: %v", err)
	}
	if err := manager.RetryDeletingMemories(ctx, MemorySelector{Scope: scope}); err != nil {
		t.Fatalf("重试删除失败: %v", err)
	}
	items, err := manager.ListMemories(ctx, MemorySelector{ID: saved.ID, Scope: scope, Status: LongTermStatusDeleted, IncludeDeleted: true})
	if err != nil {
		t.Fatalf("查询删除后记忆失败: %v", err)
	}
	if len(items) != 1 || items[0].Status != LongTermStatusDeleted {
		t.Fatalf("重试后应标记为 deleted，items=%+v", items)
	}
}

func TestSensitiveCandidateIsRejected(t *testing.T) {
	if isSafeLongTermContent("请记住 token: abc") {
		t.Fatal("包含 token 的内容不应写入长期记忆")
	}
	if !isSafeLongTermContent("请记住：团队默认用中文输出报告") {
		t.Fatal("普通团队偏好应允许写入长期记忆")
	}
}
