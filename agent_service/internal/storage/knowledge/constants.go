package knowledge

// Milvus 知识库使用的数据库与集合名称。
const (
	MilvusDBName         = "monitor_system_agent"
	MilvusCollectionName = "ops_docs"
)

// FileDir 是运维知识文档上传后的落盘目录，启动时由配置覆盖。
var FileDir = "./docs/"
