package agent

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"monitor-system/agent_service/internal/auth"
	"monitor-system/agent_service/internal/session/memory"

	"github.com/gin-gonic/gin"
)

func TestMemoryPolicyAPIEnablesRecall(t *testing.T) {
	gin.SetMode(gin.TestMode)
	manager := memory.NewInMemoryManager(memory.DefaultConfig())
	controller := NewV1WithMemoryManager(manager)
	router := gin.New()
	controller.RegisterRoutes(router.Group("/api/agent"))

	body := bytes.NewBufferString(`{
		"tenant_id":"tenant-a",
		"team_id":"team-a",
		"cluster_id":"cluster-a",
		"level":"cluster",
		"long_term_enabled":true,
		"write_enabled":true,
		"recall_enabled":true
	}`)
	req := httptest.NewRequest(http.MethodPut, "/api/agent/memory/policy", body)
	req.Header.Set("Content-Type", "application/json")
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, req)
	if recorder.Code != http.StatusOK {
		t.Fatalf("设置策略状态码=%d", recorder.Code)
	}

	scope := memory.MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a", SessionID: "s1"}
	if err := manager.AppendTurn(context.Background(), scope, "请记住：cluster-a 集群默认先检查 Redis 慢日志", "好的"); err != nil {
		t.Fatalf("追加会话失败: %v", err)
	}
	if err := manager.ExtractDurableMemories(context.Background(), scope); err != nil {
		t.Fatalf("抽取长期记忆失败: %v", err)
	}

	req = httptest.NewRequest(http.MethodGet, "/api/agent/memory?tenant_id=tenant-a&team_id=team-a&cluster_id=cluster-a", nil)
	recorder = httptest.NewRecorder()
	router.ServeHTTP(recorder, req)
	if recorder.Code != http.StatusOK {
		t.Fatalf("查询记忆状态码=%d", recorder.Code)
	}
	var res struct {
		Code int                     `json:"code"`
		Data []memory.LongTermMemory `json:"data"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &res); err != nil {
		t.Fatalf("解析响应失败: %v", err)
	}
	if res.Code != 0 || len(res.Data) != 1 {
		t.Fatalf("应返回一条长期记忆，code=%d len=%d body=%s", res.Code, len(res.Data), recorder.Body.String())
	}
}

func TestDeleteMemoryAPIHidesMemory(t *testing.T) {
	gin.SetMode(gin.TestMode)
	cfg := memory.DefaultConfig()
	cfg.LongTermEnabled = true
	cfg.WriteEnabled = true
	cfg.RecallEnabled = true
	manager := memory.NewInMemoryManager(cfg)
	controller := NewV1WithMemoryManager(manager)
	router := gin.New()
	controller.RegisterRoutes(router.Group("/api/agent"))
	scope := memory.MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a", SessionID: "s1"}
	if err := manager.AppendTurn(context.Background(), scope, "请记住：cluster-a 集群固定关注连接数", "好的"); err != nil {
		t.Fatalf("追加会话失败: %v", err)
	}
	if err := manager.ExtractDurableMemories(context.Background(), scope); err != nil {
		t.Fatalf("抽取长期记忆失败: %v", err)
	}
	items, err := manager.ListMemories(context.Background(), memory.MemorySelector{Scope: scope})
	if err != nil {
		t.Fatalf("查询长期记忆失败: %v", err)
	}
	if len(items) != 1 {
		t.Fatalf("测试准备应有一条长期记忆，got=%d", len(items))
	}

	req := httptest.NewRequest(http.MethodDelete, "/api/agent/memory/"+items[0].ID+"?tenant_id=tenant-a&team_id=team-a&cluster_id=cluster-a", nil)
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, req)
	if recorder.Code != http.StatusOK {
		t.Fatalf("删除记忆状态码=%d", recorder.Code)
	}

	visible, err := manager.ListMemories(context.Background(), memory.MemorySelector{Scope: scope})
	if err != nil {
		t.Fatalf("删除后查询失败: %v", err)
	}
	if len(visible) != 0 {
		t.Fatalf("删除后的记忆不应继续可见，got=%d", len(visible))
	}
}

func TestMemoryAPIDeniesMismatchedClaimScope(t *testing.T) {
	gin.SetMode(gin.TestMode)
	manager := memory.NewInMemoryManager(memory.DefaultConfig())
	controller := NewV1WithMemoryManager(manager)
	router := gin.New()
	controller.RegisterRoutes(router.Group("/api/agent"))

	req := httptest.NewRequest(http.MethodGet, "/api/agent/memory?tenant_id=tenant-b&team_id=team-a", nil)
	ctx := auth.ContextWithClaims(req.Context(), auth.Claims{
		UserID:   1,
		Username: "alice",
		Role:     "user",
		TenantID: "tenant-a",
		TeamID:   "team-a",
	})
	req = req.WithContext(ctx)
	recorder := httptest.NewRecorder()

	router.ServeHTTP(recorder, req)
	var res struct {
		Code    int    `json:"code"`
		Message string `json:"message"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &res); err != nil {
		t.Fatalf("解析响应失败: %v", err)
	}
	if res.Code == 0 {
		t.Fatalf("租户不匹配时应拒绝访问，body=%s", recorder.Body.String())
	}
}
