package agent

import (
	"monitor-system/agent_service/internal/handler/agent/dto"
	"monitor-system/agent_service/internal/session/memory"
	"strings"
)

func memoryScopeFromChatReq(req *dto.ChatReq) memory.MemoryScope {
	return memory.MemoryScope{
		TenantID:  req.TenantId,
		TeamID:    req.TeamId,
		ClusterID: req.ClusterId,
		SessionID: req.Id,
	}.Normalized()
}

func memoryScopeFromChatStreamReq(req *dto.ChatStreamReq) memory.MemoryScope {
	return memory.MemoryScope{
		TenantID:  req.TenantId,
		TeamID:    req.TeamId,
		ClusterID: req.ClusterId,
		SessionID: req.Id,
	}.Normalized()
}

func emptyMemoryContext(scope memory.MemoryScope) *memory.MemoryContext {
	return &memory.MemoryContext{
		Scope:          scope.Normalized(),
		RecentMessages: nil,
		LongTerm:       nil,
	}
}

func validateChatMemoryScope(scope memory.MemoryScope) error {
	return memory.ValidateSessionScope(scope)
}

func validateMemorySelectorScope(selector memory.MemorySelector) error {
	return memory.ValidateMemoryScope(selector.Scope)
}

func validateMemoryPolicyScope(level memory.ScopeLevel, scope memory.MemoryScope) error {
	return memory.ValidatePolicyScope(level, scope)
}

func formatLongTermForPrompt(items []memory.LongTermMemory) string {
	text := memory.FormatLongTermMemories(items)
	if strings.TrimSpace(text) == "" {
		return "无"
	}
	return text
}

func memoryScopeFromPolicyReq(req *dto.MemoryPolicyReq) memory.MemoryScope {
	return memory.MemoryScope{
		TenantID:  req.TenantId,
		TeamID:    req.TeamId,
		ClusterID: req.ClusterId,
	}.Normalized()
}

func scopeLevelFromString(value string, scope memory.MemoryScope) memory.ScopeLevel {
	switch strings.ToLower(strings.TrimSpace(value)) {
	case string(memory.ScopeLevelGlobal):
		return memory.ScopeLevelGlobal
	case string(memory.ScopeLevelTenant):
		return memory.ScopeLevelTenant
	case string(memory.ScopeLevelTeam):
		return memory.ScopeLevelTeam
	case string(memory.ScopeLevelCluster):
		return memory.ScopeLevelCluster
	default:
		return scope.MostSpecificPolicyLevel()
	}
}
