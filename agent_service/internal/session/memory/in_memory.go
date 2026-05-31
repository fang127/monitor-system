package memory

import (
	"context"
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/cloudwego/eino/schema"
	"github.com/google/uuid"
)

// InMemoryManager 提供默认可运行的记忆管理器，适合长期记忆关闭时的兼容路径和单元测试。
type InMemoryManager struct {
	cfg   MemoryConfig
	store MemoryStore
	index VectorIndex
}

// NewInMemoryManager 创建基于内存存储的记忆管理器。
func NewInMemoryManager(cfg MemoryConfig) *InMemoryManager {
	store := NewInMemoryStore()
	return NewManager(cfg, store, store)
}

// NewManager 使用指定存储和向量索引创建记忆管理器。
func NewManager(cfg MemoryConfig, store MemoryStore, index VectorIndex) *InMemoryManager {
	if cfg.RecentWindow <= 0 {
		cfg.RecentWindow = DefaultConfig().RecentWindow
	}
	if cfg.SummaryWindow <= 0 {
		cfg.SummaryWindow = DefaultConfig().SummaryWindow
	}
	if cfg.RecallLimit <= 0 {
		cfg.RecallLimit = DefaultConfig().RecallLimit
	}
	if cfg.TokenBudget <= 0 {
		cfg.TokenBudget = DefaultConfig().TokenBudget
	}
	return &InMemoryManager{cfg: cfg, store: store, index: index}
}

// LoadContext 读取会话摘要、最近消息和可召回的长期记忆。
func (m *InMemoryManager) LoadContext(ctx context.Context, scope MemoryScope, query string) (*MemoryContext, error) {
	scope = scope.Normalized()
	policy, err := ResolvePolicy(ctx, m.store, m.cfg, scope)
	if err != nil {
		return nil, err
	}
	session, err := m.store.LoadSession(ctx, scope)
	if err != nil {
		return nil, err
	}
	recent := cloneMessages(trimMessages(session.Messages, policy.RecentWindow))
	longTerm := []LongTermMemory{}
	if policy.LongTermEnabled && policy.RecallEnabled {
		longTerm, err = m.recallLongTerm(ctx, scope, query, policy)
		if err != nil {
			return nil, err
		}
	}
	return &MemoryContext{
		Scope:          scope,
		Policy:         policy,
		Summary:        session.Summary,
		RecentMessages: recent,
		LongTerm:       longTerm,
	}, nil
}

// AppendTurn 追加一轮用户和助手消息，并更新会话摘要。
func (m *InMemoryManager) AppendTurn(ctx context.Context, scope MemoryScope, userMessage string, assistantMessage string) error {
	scope = scope.Normalized()
	policy, err := ResolvePolicy(ctx, m.store, m.cfg, scope)
	if err != nil {
		return err
	}
	session, err := m.store.LoadSession(ctx, scope)
	if err != nil {
		return err
	}
	if strings.TrimSpace(userMessage) != "" {
		session.Messages = append(session.Messages, schema.UserMessage(strings.TrimSpace(userMessage)))
	}
	if strings.TrimSpace(assistantMessage) != "" {
		session.Messages = append(session.Messages, schema.AssistantMessage(strings.TrimSpace(assistantMessage), nil))
	}
	session.Messages = trimMessages(session.Messages, policy.RecentWindow)
	session.UpdatedAt = time.Now()
	if err := m.store.SaveSession(ctx, session); err != nil {
		return err
	}
	return m.SummarizeSession(ctx, scope)
}

// SummarizeSession 使用确定性摘要保留当前会话的关键信息。
func (m *InMemoryManager) SummarizeSession(ctx context.Context, scope MemoryScope) error {
	scope = scope.Normalized()
	policy, err := ResolvePolicy(ctx, m.store, m.cfg, scope)
	if err != nil {
		return err
	}
	session, err := m.store.LoadSession(ctx, scope)
	if err != nil {
		return err
	}
	window := trimMessages(session.Messages, policy.SummaryWindow)
	if len(window) == 0 {
		return nil
	}
	lines := make([]string, 0, len(window))
	for _, msg := range window {
		role := "助手"
		if msg.Role == schema.User {
			role = "用户"
		}
		content := strings.TrimSpace(msg.Content)
		if content == "" {
			continue
		}
		lines = append(lines, fmt.Sprintf("%s：%s", role, content))
	}
	summary := strings.Join(lines, "\n")
	if len([]rune(summary)) > policy.TokenBudget {
		runes := []rune(summary)
		summary = string(runes[len(runes)-policy.TokenBudget:])
	}
	session.Summary = summary
	session.UpdatedAt = time.Now()
	return m.store.SaveSession(ctx, session)
}

// ExtractDurableMemories 从当前会话中抽取候选长期记忆，默认只接受稳定且低风险的显式表达。
func (m *InMemoryManager) ExtractDurableMemories(ctx context.Context, scope MemoryScope) error {
	scope = scope.Normalized()
	policy, err := ResolvePolicy(ctx, m.store, m.cfg, scope)
	if err != nil {
		return err
	}
	if !policy.LongTermEnabled || !policy.WriteEnabled {
		return nil
	}
	session, err := m.store.LoadSession(ctx, scope)
	if err != nil {
		return err
	}
	candidates := extractCandidates(scope, session.Messages)
	for _, candidate := range candidates {
		if !isSafeLongTermContent(candidate.Content) {
			continue
		}
		saved, err := m.store.SaveLongTermMemory(ctx, candidate)
		if err != nil {
			return err
		}
		if err := m.index.Upsert(ctx, saved); err != nil {
			return err
		}
		_ = m.store.RecordMemoryEvent(ctx, MemoryEvent{
			ID:        uuid.NewString(),
			MemoryID:  saved.ID,
			Scope:     scope,
			Action:    "create",
			Detail:    "从会话中自动抽取长期记忆",
			CreatedAt: time.Now(),
		})
	}
	return nil
}

// SetLongTermEnabled 同时设置某个作用域的长期记忆写入和召回开关。
func (m *InMemoryManager) SetLongTermEnabled(ctx context.Context, scope MemoryScope, enabled bool) error {
	level := scope.MostSpecificPolicyLevel()
	return m.SetPolicy(ctx, level, scope, MemoryPolicy{
		LongTermEnabled: boolPtr(enabled),
		WriteEnabled:    boolPtr(enabled),
		RecallEnabled:   boolPtr(enabled),
	})
}

// SetPolicy 设置指定层级的策略覆盖项。
func (m *InMemoryManager) SetPolicy(ctx context.Context, level ScopeLevel, scope MemoryScope, policy MemoryPolicy) error {
	return m.store.SetPolicy(ctx, level, scope, policy)
}

// ListMemories 查询长期记忆。
func (m *InMemoryManager) ListMemories(ctx context.Context, selector MemorySelector) ([]LongTermMemory, error) {
	return m.store.ListLongTermMemories(ctx, selector)
}

// DeleteMemories 删除长期记忆并清理向量索引。
func (m *InMemoryManager) DeleteMemories(ctx context.Context, selector MemorySelector) error {
	items, err := m.store.ListLongTermMemories(ctx, selector)
	if err != nil {
		return err
	}
	vectorIDs := make([]string, 0, len(items))
	for _, item := range items {
		if err := m.store.MarkLongTermStatus(ctx, item.ID, LongTermStatusDeleting); err != nil {
			return err
		}
		if item.VectorID != "" {
			vectorIDs = append(vectorIDs, item.VectorID)
		}
	}
	if err := m.index.Delete(ctx, vectorIDs); err != nil {
		return err
	}
	for _, item := range items {
		if err := m.store.MarkLongTermStatus(ctx, item.ID, LongTermStatusDeleted); err != nil {
			return err
		}
		_ = m.store.RecordMemoryEvent(ctx, MemoryEvent{
			ID:        uuid.NewString(),
			MemoryID:  item.ID,
			Scope:     item.Scope,
			Action:    "delete",
			Detail:    "删除长期记忆",
			CreatedAt: time.Now(),
		})
	}
	return nil
}

// RetryDeletingMemories 重试处于 deleting 状态的记忆向量清理，并在成功后标记为 deleted。
func (m *InMemoryManager) RetryDeletingMemories(ctx context.Context, selector MemorySelector) error {
	selector.Status = LongTermStatusDeleting
	selector.IncludeDeleted = true
	items, err := m.store.ListLongTermMemories(ctx, selector)
	if err != nil {
		return err
	}
	vectorIDs := make([]string, 0, len(items))
	for _, item := range items {
		if item.VectorID != "" {
			vectorIDs = append(vectorIDs, item.VectorID)
		}
	}
	if err := m.index.Delete(ctx, vectorIDs); err != nil {
		return err
	}
	for _, item := range items {
		if err := m.store.MarkLongTermStatus(ctx, item.ID, LongTermStatusDeleted); err != nil {
			return err
		}
		_ = m.store.RecordMemoryEvent(ctx, MemoryEvent{
			ID:        uuid.NewString(),
			MemoryID:  item.ID,
			Scope:     item.Scope,
			Action:    "delete_retry",
			Detail:    "重试清理长期记忆向量索引",
			CreatedAt: time.Now(),
		})
	}
	return nil
}

func (m *InMemoryManager) recallLongTerm(ctx context.Context, scope MemoryScope, query string, policy EffectivePolicy) ([]LongTermMemory, error) {
	ids, err := m.index.Search(ctx, MemorySelector{
		Scope: scope,
		Query: query,
		Limit: policy.RecallLimit,
	})
	if err != nil {
		return nil, err
	}
	byID := map[string]struct{}{}
	for _, id := range ids {
		byID[id] = struct{}{}
	}
	items, err := m.store.ListLongTermMemories(ctx, MemorySelector{
		Scope:  scope,
		Status: LongTermStatusActive,
		Limit:  policy.RecallLimit,
	})
	if err != nil {
		return nil, err
	}
	if len(byID) == 0 {
		return items, nil
	}
	filtered := make([]LongTermMemory, 0, len(items))
	for _, item := range items {
		if _, ok := byID[item.ID]; ok {
			now := time.Now()
			item.LastUsedAt = &now
			filtered = append(filtered, item)
		}
	}
	return filtered, nil
}

// InMemoryStore 是线程安全的内存记忆存储。
type InMemoryStore struct {
	mu       sync.RWMutex
	sessions map[string]*SessionMemory
	policies map[string]MemoryPolicy
	longTerm map[string]LongTermMemory
	events   []MemoryEvent
}

// NewInMemoryStore 创建内存存储。
func NewInMemoryStore() *InMemoryStore {
	return &InMemoryStore{
		sessions: map[string]*SessionMemory{},
		policies: map[string]MemoryPolicy{},
		longTerm: map[string]LongTermMemory{},
		events:   []MemoryEvent{},
	}
}

// LoadSession 读取或创建会话记忆。
func (s *InMemoryStore) LoadSession(ctx context.Context, scope MemoryScope) (*SessionMemory, error) {
	_ = ctx
	scope = scope.Normalized()
	key := scope.SessionKey()
	s.mu.RLock()
	session, ok := s.sessions[key]
	s.mu.RUnlock()
	if ok {
		return cloneSession(session), nil
	}
	return &SessionMemory{Scope: scope, Messages: []*schema.Message{}, UpdatedAt: time.Now()}, nil
}

// SaveSession 保存会话记忆。
func (s *InMemoryStore) SaveSession(ctx context.Context, session *SessionMemory) error {
	_ = ctx
	if session == nil {
		return nil
	}
	session.Scope = session.Scope.Normalized()
	s.mu.Lock()
	defer s.mu.Unlock()
	s.sessions[session.Scope.SessionKey()] = cloneSession(session)
	return nil
}

// GetPolicy 读取策略覆盖项。
func (s *InMemoryStore) GetPolicy(ctx context.Context, level ScopeLevel, scope MemoryScope) (*MemoryPolicy, error) {
	_ = ctx
	key := scope.PolicyKey(level)
	s.mu.RLock()
	defer s.mu.RUnlock()
	policy, ok := s.policies[key]
	if !ok {
		return nil, nil
	}
	return &policy, nil
}

// SetPolicy 保存策略覆盖项。
func (s *InMemoryStore) SetPolicy(ctx context.Context, level ScopeLevel, scope MemoryScope, policy MemoryPolicy) error {
	_ = ctx
	key := scope.PolicyKey(level)
	s.mu.Lock()
	defer s.mu.Unlock()
	s.policies[key] = policy
	return nil
}

// SaveLongTermMemory 保存长期记忆。
func (s *InMemoryStore) SaveLongTermMemory(ctx context.Context, item LongTermMemory) (LongTermMemory, error) {
	_ = ctx
	now := time.Now()
	if item.ID == "" {
		item.ID = uuid.NewString()
	}
	if item.VectorID == "" {
		item.VectorID = item.ID
	}
	if item.Status == "" {
		item.Status = LongTermStatusActive
	}
	if item.CreatedAt.IsZero() {
		item.CreatedAt = now
	}
	item.UpdatedAt = now
	item.Scope = item.Scope.Normalized()
	s.mu.Lock()
	defer s.mu.Unlock()
	s.longTerm[item.ID] = item
	return item, nil
}

// ListLongTermMemories 查询长期记忆。
func (s *InMemoryStore) ListLongTermMemories(ctx context.Context, selector MemorySelector) ([]LongTermMemory, error) {
	_ = ctx
	selector.Scope = selector.Scope.Normalized()
	now := time.Now()
	s.mu.RLock()
	defer s.mu.RUnlock()
	items := make([]LongTermMemory, 0, len(s.longTerm))
	for _, item := range s.longTerm {
		if !matchMemory(selector, item, now) {
			continue
		}
		items = append(items, item)
	}
	sort.SliceStable(items, func(i, j int) bool {
		if items[i].Scope.ClusterID != items[j].Scope.ClusterID {
			return items[i].Scope.ClusterID != ""
		}
		return items[i].UpdatedAt.After(items[j].UpdatedAt)
	})
	if selector.Limit > 0 && len(items) > selector.Limit {
		items = items[:selector.Limit]
	}
	return items, nil
}

// MarkLongTermStatus 更新长期记忆状态。
func (s *InMemoryStore) MarkLongTermStatus(ctx context.Context, id string, status LongTermStatus) error {
	_ = ctx
	s.mu.Lock()
	defer s.mu.Unlock()
	item, ok := s.longTerm[id]
	if !ok {
		return fmt.Errorf("长期记忆 %q 不存在", id)
	}
	item.Status = status
	item.UpdatedAt = time.Now()
	s.longTerm[id] = item
	return nil
}

// RecordMemoryEvent 记录记忆事件。
func (s *InMemoryStore) RecordMemoryEvent(ctx context.Context, event MemoryEvent) error {
	_ = ctx
	s.mu.Lock()
	defer s.mu.Unlock()
	s.events = append(s.events, event)
	return nil
}

// Upsert 写入内存向量索引。内存实现无需额外动作。
func (s *InMemoryStore) Upsert(ctx context.Context, item LongTermMemory) error {
	_ = ctx
	_ = item
	return nil
}

// Search 通过简单文本匹配模拟语义召回，真实环境可替换为 Milvus 实现。
func (s *InMemoryStore) Search(ctx context.Context, selector MemorySelector) ([]string, error) {
	items, err := s.ListLongTermMemories(ctx, MemorySelector{
		Scope:  selector.Scope,
		Status: LongTermStatusActive,
		Limit:  selector.Limit,
	})
	if err != nil {
		return nil, err
	}
	query := strings.ToLower(strings.TrimSpace(selector.Query))
	ids := make([]string, 0, len(items))
	for _, item := range items {
		content := strings.ToLower(item.Content)
		if query == "" || strings.Contains(content, query) || hasSharedToken(query, content) {
			ids = append(ids, item.ID)
		}
	}
	if selector.Limit > 0 && len(ids) > selector.Limit {
		ids = ids[:selector.Limit]
	}
	return ids, nil
}

// Delete 清理内存向量索引。内存实现无需额外动作。
func (s *InMemoryStore) Delete(ctx context.Context, ids []string) error {
	_ = ctx
	_ = ids
	return nil
}

func matchMemory(selector MemorySelector, item LongTermMemory, now time.Time) bool {
	if selector.ID != "" && item.ID != selector.ID {
		return false
	}
	if selector.Scope.TenantID != "" && item.Scope.TenantID != selector.Scope.TenantID {
		return false
	}
	if selector.Scope.TeamID != "" && item.Scope.TeamID != selector.Scope.TeamID {
		return false
	}
	if selector.Scope.ClusterID != "" && item.Scope.ClusterID != selector.Scope.ClusterID {
		return false
	}
	if selector.Type != "" && item.Type != selector.Type {
		return false
	}
	if selector.Status != "" && item.Status != selector.Status {
		return false
	}
	if !selector.IncludeDeleted && (item.Status == LongTermStatusDeleted || item.Status == LongTermStatusDeleting) {
		return false
	}
	if item.Status != LongTermStatusActive && selector.Status == "" {
		return false
	}
	if item.ExpiresAt != nil && item.ExpiresAt.Before(now) {
		return false
	}
	return true
}

func trimMessages(messages []*schema.Message, limit int) []*schema.Message {
	if limit <= 0 || len(messages) <= limit {
		return cloneMessages(messages)
	}
	return cloneMessages(messages[len(messages)-limit:])
}

func cloneSession(session *SessionMemory) *SessionMemory {
	if session == nil {
		return &SessionMemory{Messages: []*schema.Message{}}
	}
	return &SessionMemory{
		Scope:     session.Scope,
		Summary:   session.Summary,
		Messages:  cloneMessages(session.Messages),
		UpdatedAt: session.UpdatedAt,
	}
}

func cloneMessages(messages []*schema.Message) []*schema.Message {
	cloned := make([]*schema.Message, len(messages))
	copy(cloned, messages)
	return cloned
}

func hasSharedToken(query string, content string) bool {
	for _, token := range strings.Fields(query) {
		token = strings.TrimSpace(token)
		if len([]rune(token)) < 2 {
			continue
		}
		if strings.Contains(content, token) {
			return true
		}
	}
	return false
}
