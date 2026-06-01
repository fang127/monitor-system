package milvus_config

// Milvus 知识库使用的数据库与集合名称。
const (
	MilvusDBName           = "monitor_system_agent"
	AgentOpsDocsCollection = "ops_docs"
	// LongTermMemoryCollection 是长期记忆专用向量集合，必须和内部文档知识库集合隔离。
	LongTermMemoryCollection = "agent_long_term_memories"
)

// FileDir 是运维知识文档上传后的落盘目录，启动时由配置覆盖。
var FileDir = "./docs/"
