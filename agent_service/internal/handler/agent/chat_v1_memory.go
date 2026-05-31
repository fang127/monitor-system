package agent

import (
	"fmt"
	"monitor-system/agent_service/internal/auth"
	"monitor-system/agent_service/internal/handler/agent/dto"
	"monitor-system/agent_service/internal/response"
	"monitor-system/agent_service/internal/session/memory"
	"strings"

	"github.com/gin-gonic/gin"
)

// ListMemories 查询长期记忆列表或单条长期记忆。
func (c *ControllerV1) ListMemories(gctx *gin.Context) {
	selector := memorySelectorFromQuery(gctx)
	if err := requireMemoryScopeAllowed(gctx, selector.Scope); err != nil {
		response.Respond(gctx, nil, err)
		return
	}
	items, err := c.memoryManager.ListMemories(gctx.Request.Context(), selector)
	response.Respond(gctx, items, err)
}

// SetMemoryPolicy 设置长期记忆策略。
func (c *ControllerV1) SetMemoryPolicy(gctx *gin.Context) {
	var req dto.MemoryPolicyReq
	if err := gctx.ShouldBindJSON(&req); err != nil {
		response.Respond(gctx, nil, fmt.Errorf("解析请求失败: %w", err))
		return
	}
	scope := memoryScopeFromPolicyReq(&req)
	if err := requireMemoryScopeAllowed(gctx, scope); err != nil {
		response.Respond(gctx, nil, err)
		return
	}
	level := scopeLevelFromString(req.Level, scope)
	policy := memory.MemoryPolicy{
		LongTermEnabled: req.LongTermEnabled,
		WriteEnabled:    req.WriteEnabled,
		RecallEnabled:   req.RecallEnabled,
		RecentWindow:    req.RecentWindow,
		SummaryWindow:   req.SummaryWindow,
		RecallLimit:     req.RecallLimit,
		TokenBudget:     req.TokenBudget,
	}
	err := c.memoryManager.SetPolicy(gctx.Request.Context(), level, scope, policy)
	response.Respond(gctx, gin.H{"level": level, "scope": scope}, err)
}

// DeleteMemory 按记忆 ID 删除长期记忆。
func (c *ControllerV1) DeleteMemory(gctx *gin.Context) {
	selector := memorySelectorFromQuery(gctx)
	if err := requireMemoryScopeAllowed(gctx, selector.Scope); err != nil {
		response.Respond(gctx, nil, err)
		return
	}
	selector.ID = strings.TrimSpace(gctx.Param("id"))
	if selector.ID == "" {
		response.Respond(gctx, nil, fmt.Errorf("记忆 ID 不能为空"))
		return
	}
	err := c.memoryManager.DeleteMemories(gctx.Request.Context(), selector)
	response.Respond(gctx, gin.H{"deleted": selector.ID}, err)
}

// ClearMemories 按作用域清空长期记忆。
func (c *ControllerV1) ClearMemories(gctx *gin.Context) {
	selector := memorySelectorFromQuery(gctx)
	if err := requireMemoryScopeAllowed(gctx, selector.Scope); err != nil {
		response.Respond(gctx, nil, err)
		return
	}
	err := c.memoryManager.DeleteMemories(gctx.Request.Context(), selector)
	response.Respond(gctx, gin.H{"scope": selector.Scope}, err)
}

func memorySelectorFromQuery(gctx *gin.Context) memory.MemorySelector {
	scope := memory.MemoryScope{
		TenantID:  gctx.Query("tenant_id"),
		TeamID:    gctx.Query("team_id"),
		ClusterID: gctx.Query("cluster_id"),
		SessionID: gctx.Query("session_id"),
	}.Normalized()
	status := memory.LongTermStatus(strings.TrimSpace(gctx.Query("status")))
	return memory.MemorySelector{
		ID:             strings.TrimSpace(gctx.Query("id")),
		Scope:          scope,
		Type:           strings.TrimSpace(gctx.Query("type")),
		Status:         status,
		IncludeDeleted: strings.EqualFold(gctx.Query("include_deleted"), "true"),
		Query:          strings.TrimSpace(gctx.Query("query")),
	}
}

func requireMemoryScopeAllowed(gctx *gin.Context, scope memory.MemoryScope) error {
	claims, ok := auth.ClaimsFromContext(gctx.Request.Context())
	if !ok {
		return nil
	}
	if strings.EqualFold(claims.Role, "admin") {
		return nil
	}
	if claims.TenantID != "" && claims.TenantID != scope.TenantID {
		return fmt.Errorf("无权访问租户 %s 的记忆", scope.TenantID)
	}
	if claims.TeamID != "" && claims.TeamID != scope.TeamID {
		return fmt.Errorf("无权访问团队 %s 的记忆", scope.TeamID)
	}
	return nil
}
