package memory

const (
	// MemoryScopesTable 保存租户、团队和集群级记忆策略。
	MemoryScopesTable = "agent_memory_scopes"
	// SessionMemoriesTable 保存会话摘要和短期窗口元信息。
	SessionMemoriesTable = "agent_session_memories"
	// LongTermMemoriesTable 保存长期记忆正文和治理字段。
	LongTermMemoriesTable = "agent_long_term_memories"
	// MemoryEventsTable 保存记忆创建、召回和删除事件。
	MemoryEventsTable = "agent_memory_events"
)
