package memory

import (
	"fmt"
	"strings"
	"time"

	"github.com/cloudwego/eino/schema"
)

// ScopeLevel 表示记忆策略或记忆内容生效的作用域层级。
type ScopeLevel string

const (
	ScopeLevelGlobal  ScopeLevel = "global"
	ScopeLevelTenant  ScopeLevel = "tenant"
	ScopeLevelTeam    ScopeLevel = "team"
	ScopeLevelCluster ScopeLevel = "cluster"
)

// MemoryScope 描述一次对话或一条长期记忆所属的租户、团队、集群和会话。
type MemoryScope struct {
	TenantID  string `json:"tenant_id"`
	TeamID    string `json:"team_id"`
	ClusterID string `json:"cluster_id,omitempty"`
	SessionID string `json:"session_id,omitempty"`
}

// Normalized 返回清理空白后的作用域，不为缺失字段补默认值。
func (s MemoryScope) Normalized() MemoryScope {
	return MemoryScope{
		TenantID:  strings.TrimSpace(s.TenantID),
		TeamID:    strings.TrimSpace(s.TeamID),
		ClusterID: strings.TrimSpace(s.ClusterID),
		SessionID: strings.TrimSpace(s.SessionID),
	}
}

// ValidateMemoryScope 校验长期记忆读写所需的租户和团队作用域。
func ValidateMemoryScope(scope MemoryScope) error {
	scope = scope.Normalized()
	if scope.TenantID == "" {
		return fmt.Errorf("租户 ID 不能为空")
	}
	if scope.TeamID == "" {
		return fmt.Errorf("团队 ID 不能为空")
	}
	return nil
}

// ValidateSessionScope 校验短期会话记忆所需的租户、团队和会话作用域。
func ValidateSessionScope(scope MemoryScope) error {
	scope = scope.Normalized()
	if err := ValidateMemoryScope(scope); err != nil {
		return err
	}
	if scope.SessionID == "" {
		return fmt.Errorf("会话 ID 不能为空")
	}
	return nil
}

// ValidatePolicyScope 按策略层级校验必需的作用域字段。
func ValidatePolicyScope(level ScopeLevel, scope MemoryScope) error {
	scope = scope.Normalized()
	switch level {
	case ScopeLevelGlobal:
		return nil
	case ScopeLevelTenant:
		if scope.TenantID == "" {
			return fmt.Errorf("租户 ID 不能为空")
		}
	case ScopeLevelTeam:
		return ValidateMemoryScope(scope)
	case ScopeLevelCluster:
		if err := ValidateMemoryScope(scope); err != nil {
			return err
		}
		if scope.ClusterID == "" {
			return fmt.Errorf("集群 ID 不能为空")
		}
	default:
		return nil
	}
	return nil
}

// SessionKey 返回短期会话记忆使用的唯一键。
func (s MemoryScope) SessionKey() string {
	scope := s.Normalized()
	return strings.Join([]string{scope.TenantID, scope.TeamID, scope.ClusterID, scope.SessionID}, "/")
}

// PolicyKey 返回指定层级的策略键。
func (s MemoryScope) PolicyKey(level ScopeLevel) string {
	scope := s.Normalized()
	switch level {
	case ScopeLevelGlobal:
		return string(ScopeLevelGlobal)
	case ScopeLevelTenant:
		return fmt.Sprintf("%s/%s", ScopeLevelTenant, scope.TenantID)
	case ScopeLevelTeam:
		return fmt.Sprintf("%s/%s/%s", ScopeLevelTeam, scope.TenantID, scope.TeamID)
	case ScopeLevelCluster:
		return fmt.Sprintf("%s/%s/%s/%s", ScopeLevelCluster, scope.TenantID, scope.TeamID, scope.ClusterID)
	default:
		return string(ScopeLevelGlobal)
	}
}

// MostSpecificPolicyLevel 根据作用域选择最具体的策略层级。
func (s MemoryScope) MostSpecificPolicyLevel() ScopeLevel {
	scope := s.Normalized()
	if scope.ClusterID != "" {
		return ScopeLevelCluster
	}
	if scope.TeamID != "" {
		return ScopeLevelTeam
	}
	if scope.TenantID != "" {
		return ScopeLevelTenant
	}
	return ScopeLevelGlobal
}

// MemoryPolicy 是可叠加的策略覆盖项，nil 表示继承更宽作用域。
type MemoryPolicy struct {
	LongTermEnabled *bool `json:"long_term_enabled,omitempty"`
	WriteEnabled    *bool `json:"write_enabled,omitempty"`
	RecallEnabled   *bool `json:"recall_enabled,omitempty"`
	RecentWindow    int   `json:"recent_window,omitempty"`
	SummaryWindow   int   `json:"summary_window,omitempty"`
	RecallLimit     int   `json:"recall_limit,omitempty"`
	TokenBudget     int   `json:"token_budget,omitempty"`
}

// EffectivePolicy 是合并后的最终记忆策略。
type EffectivePolicy struct {
	LongTermEnabled bool `json:"long_term_enabled"`
	WriteEnabled    bool `json:"write_enabled"`
	RecallEnabled   bool `json:"recall_enabled"`
	RecentWindow    int  `json:"recent_window"`
	SummaryWindow   int  `json:"summary_window"`
	RecallLimit     int  `json:"recall_limit"`
	TokenBudget     int  `json:"token_budget"`
}

// MemoryConfig 是记忆系统的默认配置。
type MemoryConfig struct {
	LongTermEnabled bool
	WriteEnabled    bool
	RecallEnabled   bool
	RecentWindow    int
	SummaryWindow   int
	RecallLimit     int
	TokenBudget     int
}

// DefaultConfig 返回长期记忆默认关闭的安全配置。
func DefaultConfig() MemoryConfig {
	return MemoryConfig{
		LongTermEnabled: false,
		WriteEnabled:    false,
		RecallEnabled:   false,
		RecentWindow:    6,
		SummaryWindow:   12,
		RecallLimit:     5,
		TokenBudget:     1200,
	}
}

// MemoryContext 是聊天流程加载到的完整记忆上下文。
type MemoryContext struct {
	Scope          MemoryScope       `json:"scope"`
	Policy         EffectivePolicy   `json:"policy"`
	Summary        string            `json:"summary"`
	RecentMessages []*schema.Message `json:"recent_messages"`
	LongTerm       []LongTermMemory  `json:"long_term"`
}

// SessionMemory 保存单个会话的短期窗口和滚动摘要。
type SessionMemory struct {
	Scope     MemoryScope       `json:"scope"`
	Summary   string            `json:"summary"`
	Messages  []*schema.Message `json:"messages"`
	UpdatedAt time.Time         `json:"updated_at"`
}

// LongTermStatus 表示长期记忆的治理状态。
type LongTermStatus string

const (
	LongTermStatusActive   LongTermStatus = "active"
	LongTermStatusPending  LongTermStatus = "pending"
	LongTermStatusDeleting LongTermStatus = "deleting"
	LongTermStatusDeleted  LongTermStatus = "deleted"
)

// LongTermMemory 表示一条可治理的长期记忆。
type LongTermMemory struct {
	ID         string         `json:"id"`
	Scope      MemoryScope    `json:"scope"`
	Type       string         `json:"type"`
	Content    string         `json:"content"`
	Source     string         `json:"source"`
	Confidence float64        `json:"confidence"`
	Status     LongTermStatus `json:"status"`
	CreatedBy  string         `json:"created_by"`
	VectorID   string         `json:"vector_id,omitempty"`
	CreatedAt  time.Time      `json:"created_at"`
	UpdatedAt  time.Time      `json:"updated_at"`
	LastUsedAt *time.Time     `json:"last_used_at,omitempty"`
	ExpiresAt  *time.Time     `json:"expires_at,omitempty"`
}

// MemorySelector 描述查看、召回或删除长期记忆时的筛选条件。
type MemorySelector struct {
	ID             string
	Scope          MemoryScope
	Type           string
	Status         LongTermStatus
	IncludeDeleted bool
	Query          string
	Limit          int
}

// MemoryEvent 记录记忆治理事件，便于后续审计。
type MemoryEvent struct {
	ID        string      `json:"id"`
	MemoryID  string      `json:"memory_id,omitempty"`
	Scope     MemoryScope `json:"scope"`
	Action    string      `json:"action"`
	Detail    string      `json:"detail,omitempty"`
	CreatedAt time.Time   `json:"created_at"`
}
