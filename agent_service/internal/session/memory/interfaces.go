package memory

import "context"

// MemoryManager 是记忆系统对聊天处理器暴露的唯一入口。
type MemoryManager interface {
	LoadContext(ctx context.Context, scope MemoryScope, query string) (*MemoryContext, error)
	AppendTurn(ctx context.Context, scope MemoryScope, userMessage string, assistantMessage string) error
	SummarizeSession(ctx context.Context, scope MemoryScope) error
	ExtractDurableMemories(ctx context.Context, scope MemoryScope) error
	SetLongTermEnabled(ctx context.Context, scope MemoryScope, enabled bool) error
	SetPolicy(ctx context.Context, level ScopeLevel, scope MemoryScope, policy MemoryPolicy) error
	ListMemories(ctx context.Context, selector MemorySelector) ([]LongTermMemory, error)
	DeleteMemories(ctx context.Context, selector MemorySelector) error
	RetryDeletingMemories(ctx context.Context, selector MemorySelector) error
}

// MemoryStore 隔离会话、长期记忆、策略和事件审计的底层存储。
type MemoryStore interface {
	LoadSession(ctx context.Context, scope MemoryScope) (*SessionMemory, error)
	SaveSession(ctx context.Context, session *SessionMemory) error
	GetPolicy(ctx context.Context, level ScopeLevel, scope MemoryScope) (*MemoryPolicy, error)
	SetPolicy(ctx context.Context, level ScopeLevel, scope MemoryScope, policy MemoryPolicy) error
	SaveLongTermMemory(ctx context.Context, item LongTermMemory) (LongTermMemory, error)
	ListLongTermMemories(ctx context.Context, selector MemorySelector) ([]LongTermMemory, error)
	MarkLongTermStatus(ctx context.Context, id string, status LongTermStatus) error
	RecordMemoryEvent(ctx context.Context, event MemoryEvent) error
}

// VectorIndex 负责长期记忆的语义索引，当前内存实现提供可测试的降级召回。
type VectorIndex interface {
	Upsert(ctx context.Context, item LongTermMemory) error
	Search(ctx context.Context, selector MemorySelector) ([]string, error)
	Delete(ctx context.Context, ids []string) error
}
