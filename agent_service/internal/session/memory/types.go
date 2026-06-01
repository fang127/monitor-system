// 提供了记忆系统核心的数据结构定义，包括作用域、策略、记忆内容和事件等，支持短期会话记忆和长期记忆的统一管理。
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
	UserID    string `json:"user_id,omitempty"`
	SessionID string `json:"session_id,omitempty"`
}

// Normalized 返回清理空白后的作用域，不为缺失字段补默认值。
func (s MemoryScope) Normalized() MemoryScope {
	return MemoryScope{
		TenantID:  strings.TrimSpace(s.TenantID),
		TeamID:    strings.TrimSpace(s.TeamID),
		ClusterID: strings.TrimSpace(s.ClusterID),
		UserID:    strings.TrimSpace(s.UserID),
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
	return strings.Join([]string{scope.TenantID, scope.TeamID, scope.ClusterID, scope.UserID, scope.SessionID}, "/")
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
	LongTermEnabled     bool
	WriteEnabled        bool
	RecallEnabled       bool
	RecentWindow        int
	SummaryWindow       int
	RecallLimit         int
	TokenBudget         int
	SessionTTLSeconds   int
	RecallMinScore      float64
	AutoActiveThreshold float64
	PendingThreshold    float64
}

// DefaultConfig 返回长期记忆默认关闭的安全配置。
func DefaultConfig() MemoryConfig {
	return MemoryConfig{
		LongTermEnabled:     false,
		WriteEnabled:        false,
		RecallEnabled:       false,
		RecentWindow:        6,
		SummaryWindow:       12,
		RecallLimit:         5,
		TokenBudget:         1200,
		SessionTTLSeconds:   7 * 24 * 3600,
		RecallMinScore:      0.35,
		AutoActiveThreshold: 0.82,
		PendingThreshold:    0.55,
	}
}

// MemoryContext 是聊天流程加载到的完整记忆上下文。
type MemoryContext struct {
	Scope          MemoryScope       `json:"scope"`           // 作用域，包含租户、团队、集群、用户和会话信息，由系统根据当前对话环境自动填充。
	Policy         EffectivePolicy   `json:"policy"`          // 当前生效的记忆策略，由系统根据作用域层级合并计算得出。
	Summary        string            `json:"summary"`         // 会话摘要，包含近期对话的核心信息，由系统维护更新。
	RecentMessages []*schema.Message `json:"recent_messages"` // 短期记忆窗口内的消息列表，按时间顺序排列，由系统维护更新。
	LongTerm       []LongTermMemory  `json:"long_term"`       // 召回的长期记忆列表，按相关度排序，由系统根据策略和查询自动更新。
}

// SessionMemory 保存单个会话的短期窗口和滚动摘要。
type SessionMemory struct {
	Scope     MemoryScope       `json:"scope"`      // 作用域，包含租户、团队、集群、用户和会话信息，由系统根据当前对话环境自动填充。
	Summary   string            `json:"summary"`    // 会话摘要，包含近期对话的核心信息，由系统维护更新。
	Messages  []*schema.Message `json:"messages"`   // 短期记忆窗口内的消息列表，按时间顺序排列，由系统维护更新。
	UpdatedAt time.Time         `json:"updated_at"` // 上次更新短期记忆的时间戳，由系统自动维护。
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
	ID              string         `json:"id"`
	Scope           MemoryScope    `json:"scope"`
	ScopeLevel      ScopeLevel     `json:"scope_level,omitempty"`
	Type            string         `json:"type"`
	Content         string         `json:"content"`
	ContentHash     string         `json:"content_hash,omitempty"`
	Source          string         `json:"source"`
	Confidence      float64        `json:"confidence"`
	Reason          string         `json:"reason,omitempty"`
	Sensitivity     string         `json:"sensitivity,omitempty"`
	ConflictOf      string         `json:"conflict_of,omitempty"`
	Status          LongTermStatus `json:"status"`
	CreatedBy       string         `json:"created_by"`
	CreatedByUserID string         `json:"created_by_user_id,omitempty"`
	VectorID        string         `json:"vector_id,omitempty"`
	CreatedAt       time.Time      `json:"created_at"`
	UpdatedAt       time.Time      `json:"updated_at"`
	LastUsedAt      *time.Time     `json:"last_used_at,omitempty"`
	ExpiresAt       *time.Time     `json:"expires_at,omitempty"`
}

// MemorySelector 描述查看、召回或删除长期记忆时的筛选条件。
type MemorySelector struct {
	ID             string         // 精确匹配记忆 ID，优先级最高。
	Scope          MemoryScope    // 作用域，包含租户、团队、集群、用户和会话信息，由系统根据当前对话环境自动填充。
	Type           string         // 记忆类型，支持模糊匹配。
	Status         LongTermStatus // 记忆状态，支持模糊匹配。
	IncludeDeleted bool           // 是否包含已删除的记忆，默认为 false。
	Query          string         // 召回查询字符串，支持模糊匹配记忆内容。
	Limit          int            // 返回的记忆数量限制，默认为 10。
	MinScore       float64        // 召回的最小相关度分数，默认为 0.35。
}

// MemoryCandidate 是结构化抽取得到的长期记忆候选。
type MemoryCandidate struct {
	Type        string     `json:"type"`
	Content     string     `json:"content"`
	ScopeLevel  ScopeLevel `json:"scope_level"`
	Confidence  float64    `json:"confidence"`
	Reason      string     `json:"reason"`
	ExpiresAt   *time.Time `json:"expires_at,omitempty"`
	Sensitivity string     `json:"sensitivity"`
	ShouldStore bool       `json:"should_store"`
}

// MemoryEvent 记录记忆治理事件，便于后续审计。
type MemoryEvent struct {
	ID          string      `json:"id"`
	MemoryID    string      `json:"memory_id,omitempty"`
	Scope       MemoryScope `json:"scope"`
	Action      string      `json:"action"`
	ActorUserID string      `json:"actor_user_id,omitempty"`
	Detail      string      `json:"detail,omitempty"`
	CreatedAt   time.Time   `json:"created_at"`
}
