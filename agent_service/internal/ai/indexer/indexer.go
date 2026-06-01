package indexer

import (
	"context"
	embedder2 "monitor-system/agent_service/internal/ai/embedder"
	storageMilvus "monitor-system/agent_service/internal/storage/milvus"
	"monitor-system/agent_service/internal/storage/milvus_config"

	einoMilvus "github.com/cloudwego/eino-ext/components/indexer/milvus"
	"github.com/milvus-io/milvus-sdk-go/v2/entity"
)

// NewMilvusIndexer 创建一个新的 Milvus 索引器实例
func NewMilvusIndexer(ctx context.Context) (*einoMilvus.Indexer, error) {
	// 创建 Milvus 客户端
	cli, err := storageMilvus.NewMilvusClient(ctx)
	if err != nil {
		return nil, err
	}
	// 创建嵌入向量生成器
	eb, err := embedder2.CrearteEmbedding(ctx)
	if err != nil {
		return nil, err
	}
	config := &einoMilvus.IndexerConfig{
		Client:     cli,
		Collection: milvus_config.AgentOpsDocsCollection,
		Fields:     fields,
		Embedding:  eb,
	}
	// 创建 Milvus 索引器实例
	indexer, err := einoMilvus.NewIndexer(ctx, config)
	if err != nil {
		return nil, err
	}
	return indexer, nil
}

// 定义 Milvus 索引器使用的字段
var fields = []*entity.Field{
	{
		Name:     "id",
		DataType: entity.FieldTypeVarChar,
		TypeParams: map[string]string{
			"max_length": "255",
		},
		PrimaryKey: true,
	},
	{
		Name:     "vector", // 确保字段名匹配
		DataType: entity.FieldTypeBinaryVector,
		TypeParams: map[string]string{
			"dim": "65536",
		},
	},
	{
		Name:     "content",
		DataType: entity.FieldTypeVarChar,
		TypeParams: map[string]string{
			"max_length": "8192",
		},
	},
	{
		Name:     "metadata",
		DataType: entity.FieldTypeJSON,
	},
}
