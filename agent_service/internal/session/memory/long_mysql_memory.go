// mysql_store.go 定义了 MySQLMemoryStore，使用 GORM 访问 MySQL 数据库，作为长期记忆、策略和审计事件的权威存储。
package memory

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/google/uuid"
	"gorm.io/gorm"
	"gorm.io/gorm/clause"
)

// MySQLMemoryStore 使用 GORM 访问 MySQL，作为长期记忆、策略和审计事件的权威存储。
type MySQLMemoryStore struct {
	db *gorm.DB
}

// NewMySQLMemoryStore 创建 MySQL 长期记忆存储。
func NewMySQLMemoryStore(db *gorm.DB) *MySQLMemoryStore {
	return &MySQLMemoryStore{db: db}
}

type agentMemoryScopeModel struct {
	ID              uint64 `gorm:"column:id;primaryKey"`
	ScopeLevel      string `gorm:"column:scope_level"`
	TenantID        string `gorm:"column:tenant_id"`
	TeamID          string `gorm:"column:team_id"`
	ClusterID       string `gorm:"column:cluster_id"`
	LongTermEnabled *bool  `gorm:"column:long_term_enabled"`
	WriteEnabled    *bool  `gorm:"column:write_enabled"`
	RecallEnabled   *bool  `gorm:"column:recall_enabled"`
	RecentWindow    *int   `gorm:"column:recent_window"`
	SummaryWindow   *int   `gorm:"column:summary_window"`
	RecallLimit     *int   `gorm:"column:recall_limit"`
	TokenBudget     *int   `gorm:"column:token_budget"`
	CreatedAt       time.Time
	UpdatedAt       time.Time
}

func (agentMemoryScopeModel) TableName() string {
	return "agent_memory_scopes"
}

type agentLongTermMemoryModel struct {
	ID              string     `gorm:"column:id;primaryKey"`
	TenantID        string     `gorm:"column:tenant_id"`
	TeamID          string     `gorm:"column:team_id"`
	ClusterID       string     `gorm:"column:cluster_id"`
	ScopeLevel      string     `gorm:"column:scope_level"`
	MemoryType      string     `gorm:"column:memory_type"`
	Content         string     `gorm:"column:content"`
	ContentHash     string     `gorm:"column:content_hash"`
	Source          string     `gorm:"column:source"`
	Confidence      float64    `gorm:"column:confidence"`
	Reason          string     `gorm:"column:reason"`
	Sensitivity     string     `gorm:"column:sensitivity"`
	ConflictOf      string     `gorm:"column:conflict_of"`
	Status          string     `gorm:"column:status"`
	CreatedBy       string     `gorm:"column:created_by"`
	CreatedByUserID *string    `gorm:"column:created_by_user_id"`
	VectorID        string     `gorm:"column:vector_id"`
	ExpiresAt       *time.Time `gorm:"column:expires_at"`
	LastUsedAt      *time.Time `gorm:"column:last_used_at"`
	CreatedAt       time.Time  `gorm:"column:created_at"`
	UpdatedAt       time.Time  `gorm:"column:updated_at"`
}

func (agentLongTermMemoryModel) TableName() string {
	return "agent_long_term_memories"
}

type agentMemoryEventModel struct {
	ID          string    `gorm:"column:id;primaryKey"`
	MemoryID    *string   `gorm:"column:memory_id"`
	TenantID    string    `gorm:"column:tenant_id"`
	TeamID      string    `gorm:"column:team_id"`
	ClusterID   string    `gorm:"column:cluster_id"`
	ActorUserID *string   `gorm:"column:actor_user_id"`
	Action      string    `gorm:"column:action"`
	Detail      string    `gorm:"column:detail"`
	CreatedAt   time.Time `gorm:"column:created_at"`
}

func (agentMemoryEventModel) TableName() string {
	return "agent_memory_events"
}

type agentSessionMemoryModel struct {
	ID             uint64    `gorm:"column:id;primaryKey"`
	TenantID       string    `gorm:"column:tenant_id"`
	TeamID         string    `gorm:"column:team_id"`
	ClusterID      string    `gorm:"column:cluster_id"`
	UserID         string    `gorm:"column:user_id"`
	SessionID      string    `gorm:"column:session_id"`
	Summary        string    `gorm:"column:summary"`
	RecentMessages string    `gorm:"column:recent_messages"`
	CreatedAt      time.Time `gorm:"column:created_at"`
	UpdatedAt      time.Time `gorm:"column:updated_at"`
}

func (agentSessionMemoryModel) TableName() string {
	return "agent_session_memories"
}

// GetPolicy 读取策略覆盖项。
func (s *MySQLMemoryStore) GetPolicy(ctx context.Context, level ScopeLevel, scope MemoryScope) (*MemoryPolicy, error) {
	if s == nil || s.db == nil {
		return nil, fmt.Errorf("MySQL 记忆存储未配置")
	}
	scope = scope.Normalized()
	// 按照层级和范围查询策略覆盖项，优先返回最具体的匹配项。
	var model agentMemoryScopeModel
	err := s.db.WithContext(ctx).
		Where("scope_level = ? AND tenant_id = ? AND team_id = ? AND cluster_id = ?", string(level), scope.TenantID, scope.TeamID, scope.ClusterID).
		First(&model).Error
	if errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	policy := MemoryPolicy{
		LongTermEnabled: model.LongTermEnabled,
		WriteEnabled:    model.WriteEnabled,
		RecallEnabled:   model.RecallEnabled,
	}
	if model.RecentWindow != nil {
		policy.RecentWindow = *model.RecentWindow
	}
	if model.SummaryWindow != nil {
		policy.SummaryWindow = *model.SummaryWindow
	}
	if model.RecallLimit != nil {
		policy.RecallLimit = *model.RecallLimit
	}
	if model.TokenBudget != nil {
		policy.TokenBudget = *model.TokenBudget
	}
	return &policy, nil
}

// SetPolicy 保存策略覆盖项。
func (s *MySQLMemoryStore) SetPolicy(ctx context.Context, level ScopeLevel, scope MemoryScope, policy MemoryPolicy) error {
	if s == nil || s.db == nil {
		return fmt.Errorf("MySQL 记忆存储未配置")
	}
	if err := ValidatePolicyScope(level, scope); err != nil {
		return err
	}
	scope = scope.Normalized()
	model := agentMemoryScopeModel{
		ScopeLevel:      string(level),
		TenantID:        scope.TenantID,
		TeamID:          scope.TeamID,
		ClusterID:       scope.ClusterID,
		LongTermEnabled: policy.LongTermEnabled,
		WriteEnabled:    policy.WriteEnabled,
		RecallEnabled:   policy.RecallEnabled,
		RecentWindow:    positiveIntPtr(policy.RecentWindow),
		SummaryWindow:   positiveIntPtr(policy.SummaryWindow),
		RecallLimit:     positiveIntPtr(policy.RecallLimit),
		TokenBudget:     positiveIntPtr(policy.TokenBudget),
	}
	// 使用 FirstOrCreate + Assign 实现有则更新，无则创建的效果，确保同一层级和范围只有一条记录。
	return s.db.WithContext(ctx).
		Where("scope_level = ? AND tenant_id = ? AND team_id = ? AND cluster_id = ?", string(level), scope.TenantID, scope.TeamID, scope.ClusterID).
		Assign(model).
		FirstOrCreate(&model).Error
}

// SaveLongTermMemory 保存长期记忆。
func (s *MySQLMemoryStore) SaveLongTermMemory(ctx context.Context, item LongTermMemory) (LongTermMemory, error) {
	if s == nil || s.db == nil {
		return LongTermMemory{}, fmt.Errorf("MySQL 记忆存储未配置")
	}
	item = normalizeLongTermMemory(item)
	if err := ValidateMemoryScope(item.Scope); err != nil {
		return LongTermMemory{}, err
	}
	model := longTermMemoryToModel(item)
	// 使用 OnConflict 实现有则更新，无则创建的效果，确保同一 ID 只有一条记录。
	err := s.db.WithContext(ctx).Clauses(clause.OnConflict{
		Columns:   []clause.Column{{Name: "id"}}, // 冲突目标为 ID 列
		UpdateAll: true,                          // 冲突时更新所有列
	}).Create(&model).Error
	return item, err
}

// ListLongTermMemories 查询长期记忆。
func (s *MySQLMemoryStore) ListLongTermMemories(ctx context.Context, selector MemorySelector) ([]LongTermMemory, error) {
	if s == nil || s.db == nil {
		return nil, fmt.Errorf("MySQL 记忆存储未配置")
	}
	selector.Scope = selector.Scope.Normalized()
	if err := ValidateMemoryScope(selector.Scope); err != nil {
		return nil, err
	}
	// 构建查询条件，支持按照范围、ID、类型、状态和文本搜索过滤，并排除已过期和已删除的记录。
	query := s.db.WithContext(ctx).Model(&agentLongTermMemoryModel{}).
		Where("tenant_id = ? AND team_id = ? AND cluster_id = ?", selector.Scope.TenantID, selector.Scope.TeamID, selector.Scope.ClusterID).
		Where("expires_at IS NULL OR expires_at > ?", time.Now())
	// 根据选择器的不同字段构建查询条件，支持 ID、类型、状态和文本搜索过滤，并默认排除已删除的记录。
	if selector.ID != "" {
		query = query.Where("id = ?", selector.ID)
	}
	if selector.Type != "" {
		query = query.Where("memory_type = ?", selector.Type)
	}
	if selector.Status != "" {
		query = query.Where("status = ?", string(selector.Status))
	} else if !selector.IncludeDeleted {
		query = query.Where("status = ?", string(LongTermStatusActive))
	}
	if !selector.IncludeDeleted {
		query = query.Where("status NOT IN ?", []string{string(LongTermStatusDeleting), string(LongTermStatusDeleted)})
	}
	if strings.TrimSpace(selector.Query) != "" {
		like := "%" + strings.TrimSpace(selector.Query) + "%"
		query = query.Where("content LIKE ? OR reason LIKE ? OR memory_type LIKE ?", like, like, like)
	}
	limit := selector.Limit
	if limit <= 0 {
		limit = 20
	}
	var models []agentLongTermMemoryModel
	if err := query.Order("updated_at DESC").Limit(limit).Find(&models).Error; err != nil {
		return nil, err
	}
	items := make([]LongTermMemory, 0, len(models))
	for _, model := range models {
		items = append(items, modelToLongTermMemory(model))
	}
	return items, nil
}

// MarkLongTermStatus 更新长期记忆状态。
func (s *MySQLMemoryStore) MarkLongTermStatus(ctx context.Context, id string, status LongTermStatus) error {
	if s == nil || s.db == nil {
		return fmt.Errorf("MySQL 记忆存储未配置")
	}
	result := s.db.WithContext(ctx).Model(&agentLongTermMemoryModel{}).
		Where("id = ?", id).
		Updates(map[string]any{"status": string(status), "updated_at": time.Now()})
	if result.Error != nil {
		return result.Error
	}
	if result.RowsAffected == 0 {
		return fmt.Errorf("长期记忆 %q 不存在", id)
	}
	return nil
}

// RecordMemoryEvent 记录记忆治理事件。
func (s *MySQLMemoryStore) RecordMemoryEvent(ctx context.Context, event MemoryEvent) error {
	if s == nil || s.db == nil {
		return fmt.Errorf("MySQL 记忆存储未配置")
	}
	if event.ID == "" {
		event.ID = uuid.NewString()
	}
	if event.CreatedAt.IsZero() {
		event.CreatedAt = time.Now()
	}
	scope := event.Scope.Normalized()
	model := agentMemoryEventModel{
		ID:          event.ID,
		MemoryID:    nonEmptyStringPtr(event.MemoryID),
		TenantID:    scope.TenantID,
		TeamID:      scope.TeamID,
		ClusterID:   scope.ClusterID,
		ActorUserID: nonEmptyStringPtr(event.ActorUserID),
		Action:      event.Action,
		Detail:      event.Detail,
		CreatedAt:   event.CreatedAt,
	}
	return s.db.WithContext(ctx).Create(&model).Error
}

// MySQLSessionCheckpointStore 可低频保存摘要快照，但不作为最近消息热路径。
type MySQLSessionCheckpointStore struct {
	db *gorm.DB
}

// NewMySQLSessionCheckpointStore 创建会话摘要 checkpoint 存储。
func NewMySQLSessionCheckpointStore(db *gorm.DB) *MySQLSessionCheckpointStore {
	return &MySQLSessionCheckpointStore{db: db}
}

// SaveCheckpoint 保存会话摘要快照。
// 注意：该方法不应频繁调用，避免对数据库造成过大压力。它主要用于在会话结束时保存最终摘要，或在长会话中定期保存中间摘要，以防止数据丢失。
// TODO：可以用RabbitMQ等消息队列异步处理checkpoint保存，减少对数据库的直接压力。
func (s *MySQLSessionCheckpointStore) SaveCheckpoint(ctx context.Context, session *SessionMemory) error {
	if s == nil || s.db == nil || session == nil {
		return nil
	}
	payload, err := json.Marshal(trimMessages(session.Messages, 0))
	if err != nil {
		return err
	}
	scope := session.Scope.Normalized()
	model := agentSessionMemoryModel{
		TenantID:       scope.TenantID,
		TeamID:         scope.TeamID,
		ClusterID:      scope.ClusterID,
		UserID:         scope.UserID,
		SessionID:      scope.SessionID,
		Summary:        session.Summary,
		RecentMessages: string(payload),
	}
	// 使用 OnConflict 实现有则更新，无则创建的效果，确保同一会话只有一条记录。
	return s.db.WithContext(ctx).
		Where("tenant_id = ? AND team_id = ? AND cluster_id = ? AND user_id = ? AND session_id = ?", scope.TenantID, scope.TeamID, scope.ClusterID, scope.UserID, scope.SessionID).
		Assign(model).
		FirstOrCreate(&model).Error
}

func normalizeLongTermMemory(item LongTermMemory) LongTermMemory {
	now := time.Now()
	item.Scope = item.Scope.Normalized()
	if item.ID == "" {
		item.ID = uuid.NewString()
	}
	if item.VectorID == "" {
		item.VectorID = item.ID
	}
	if item.Status == "" {
		item.Status = LongTermStatusPending
	}
	if item.ScopeLevel == "" {
		item.ScopeLevel = item.Scope.MostSpecificPolicyLevel()
	}
	if item.ContentHash == "" {
		item.ContentHash = hashContent(item.Content)
	}
	if item.CreatedAt.IsZero() {
		item.CreatedAt = now
	}
	item.UpdatedAt = now
	return item
}

func longTermMemoryToModel(item LongTermMemory) agentLongTermMemoryModel {
	return agentLongTermMemoryModel{
		ID:              item.ID,
		TenantID:        item.Scope.TenantID,
		TeamID:          item.Scope.TeamID,
		ClusterID:       item.Scope.ClusterID,
		ScopeLevel:      string(item.ScopeLevel),
		MemoryType:      item.Type,
		Content:         item.Content,
		ContentHash:     item.ContentHash,
		Source:          item.Source,
		Confidence:      item.Confidence,
		Reason:          item.Reason,
		Sensitivity:     item.Sensitivity,
		ConflictOf:      item.ConflictOf,
		Status:          string(item.Status),
		CreatedBy:       item.CreatedBy,
		CreatedByUserID: nonEmptyStringPtr(item.CreatedByUserID),
		VectorID:        item.VectorID,
		ExpiresAt:       item.ExpiresAt,
		LastUsedAt:      item.LastUsedAt,
		CreatedAt:       item.CreatedAt,
		UpdatedAt:       item.UpdatedAt,
	}
}

func modelToLongTermMemory(model agentLongTermMemoryModel) LongTermMemory {
	createdByUserID := ""
	if model.CreatedByUserID != nil {
		createdByUserID = *model.CreatedByUserID
	}
	return LongTermMemory{
		ID: model.ID,
		Scope: MemoryScope{
			TenantID:  model.TenantID,
			TeamID:    model.TeamID,
			ClusterID: model.ClusterID,
		},
		ScopeLevel:      ScopeLevel(model.ScopeLevel),
		Type:            model.MemoryType,
		Content:         model.Content,
		ContentHash:     model.ContentHash,
		Source:          model.Source,
		Confidence:      model.Confidence,
		Reason:          model.Reason,
		Sensitivity:     model.Sensitivity,
		ConflictOf:      model.ConflictOf,
		Status:          LongTermStatus(model.Status),
		CreatedBy:       model.CreatedBy,
		CreatedByUserID: createdByUserID,
		VectorID:        model.VectorID,
		CreatedAt:       model.CreatedAt,
		UpdatedAt:       model.UpdatedAt,
		LastUsedAt:      model.LastUsedAt,
		ExpiresAt:       model.ExpiresAt,
	}
}

// positiveIntPtr 返回正整数的指针，非正整数返回 nil。
func positiveIntPtr(value int) *int {
	if value <= 0 {
		return nil
	}
	return &value
}

// nonEmptyStringPtr 返回非空字符串的指针，空字符串返回 nil。
func nonEmptyStringPtr(value string) *string {
	if strings.TrimSpace(value) == "" {
		return nil
	}
	return &value
}
