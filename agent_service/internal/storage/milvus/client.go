package milvus

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/config"
	"monitor-system/agent_service/internal/storage/milvus_config"

	cli "github.com/milvus-io/milvus-sdk-go/v2/client"
	"github.com/milvus-io/milvus-sdk-go/v2/entity"
)

// 创建 Milvus 客户端，并确保知识库 collection 和索引存在。
func NewMilvusClient(ctx context.Context) (cli.Client, error) {
	return NewMilvusClientForCollection(ctx, milvus_config.AgentOpsDocsCollection)
}

// NewMilvusClientForCollection 创建 Milvus 客户端，并确保指定 collection 和索引存在。
func NewMilvusClientForCollection(ctx context.Context, collectionName string) (cli.Client, error) {
	address, err := config.ConfigString(ctx, "milvus_addr", "MILVUS_ADDR", "127.0.0.1:19530")
	if err != nil {
		return nil, err
	}
	if collectionName == "" {
		collectionName = milvus_config.AgentOpsDocsCollection
	}
	// 1. 先连接default数据库
	defaultClient, err := cli.NewClient(ctx, cli.Config{
		Address: address,
		DBName:  "default",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to connect to default database: %w", err)
	}
	defer defaultClient.Close()

	// 2. 检查agent数据库是否存在，不存在则创建
	databases, err := defaultClient.ListDatabases(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to list databases: %w", err)
	}
	agentDBExists := false
	for _, db := range databases {
		if db.Name == milvus_config.MilvusDBName {
			agentDBExists = true
			break
		}
	}
	if !agentDBExists {
		err = defaultClient.CreateDatabase(ctx, milvus_config.MilvusDBName)
		if err != nil {
			return nil, fmt.Errorf("failed to create agent database: %w", err)
		}
	}

	// 3. 创建连接到agent数据库的客户端
	agentClient, err := cli.NewClient(ctx, cli.Config{
		Address: address,
		DBName:  milvus_config.MilvusDBName,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to connect to agent database: %w", err)
	}
	agentClientReady := false
	defer func() {
		if !agentClientReady {
			agentClient.Close()
		}
	}()

	// 4. 检查业务 collection 是否存在，不存在则创建
	collections, err := agentClient.ListCollections(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to list collections: %w", err)
	}

	collectionExists := false
	for _, collection := range collections {
		if collection.Name == collectionName {
			collectionExists = true
			break
		}
	}

	if !collectionExists {
		// 创建业务 collection 的 schema。
		schema := &entity.Schema{
			CollectionName: collectionName,
			Description:    collectionDescription(collectionName),
			Fields:         fields,
		}

		// 创建 collection
		err = agentClient.CreateCollection(ctx, schema, entity.DefaultShardNumber)
		if err != nil {
			return nil, fmt.Errorf("failed to create collection %s: %w", collectionName, err)
		}

		// 为 vector 字段创建 autoindex 索引。
		vectorIndex, err := entity.NewIndexAUTOINDEX(entity.HAMMING)
		if err != nil {
			return nil, fmt.Errorf("failed to create vector index: %w", err)
		}

		// 创建 index
		err = agentClient.CreateIndex(ctx, collectionName, "vector", vectorIndex, false)
		if err != nil {
			return nil, fmt.Errorf("failed to create vector index: %w", err)
		}
	}

	if err := ensureCollectionLoaded(ctx, agentClient, collectionName); err != nil {
		return nil, err
	}

	agentClientReady = true
	return agentClient, nil
}

func collectionDescription(collectionName string) string {
	if collectionName == milvus_config.LongTermMemoryCollection {
		return "Agent long term memory collection"
	}
	return "Business knowledge collection"
}

// 确保 collection 处于加载状态，如果未加载则触发加载。
func ensureCollectionLoaded(ctx context.Context, agentClient cli.Client, collectionName string) error {
	state, err := agentClient.GetLoadState(ctx, collectionName, nil)
	if err != nil {
		return fmt.Errorf("failed to get collection %s load state: %w", collectionName, err)
	}

	if state == entity.LoadStateLoaded {
		return nil

	}

	if err := agentClient.LoadCollection(ctx, collectionName, false); err != nil {
		return fmt.Errorf("failed to load collection %s: %w", collectionName, err)
	}
	return nil
}

// 定义 collection schema 的字段。
var fields = []*entity.Field{
	{
		Name:     "id", // 字符串主键
		DataType: entity.FieldTypeVarChar,
		TypeParams: map[string]string{
			"max_length": "256", // 根据实际需求调整主键长度
		},
		PrimaryKey: true,
	},
	{
		Name:     "vector", // 二进制向量，维度 65536
		DataType: entity.FieldTypeBinaryVector,
		TypeParams: map[string]string{
			"dim": "65536",
		},
	},
	{
		Name:     "content", // 文本内容，最大长度 8192
		DataType: entity.FieldTypeVarChar,
		TypeParams: map[string]string{
			"max_length": "8192",
		},
	},
	{ // 额外的metadata字段，存储JSON格式的元信息
		Name:     "metadata",
		DataType: entity.FieldTypeJSON,
	},
}
