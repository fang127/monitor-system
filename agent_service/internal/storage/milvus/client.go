package milvus

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/config"
	"monitor-system/agent_service/internal/storage/knowledge"
	"time"

	cli "github.com/milvus-io/milvus-sdk-go/v2/client"
	"github.com/milvus-io/milvus-sdk-go/v2/entity"
)

// 创建 Milvus 客户端，并确保数据库、collection 和索引存在。

func NewMilvusClient(ctx context.Context) (cli.Client, error) {
	address, err := config.ConfigString(ctx, "milvus_addr", "MILVUS_ADDR", "127.0.0.1:19530")
	if err != nil {
		return nil, err
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
		if db.Name == knowledge.MilvusDBName {
			agentDBExists = true
			break
		}
	}
	if !agentDBExists {
		err = defaultClient.CreateDatabase(ctx, knowledge.MilvusDBName)
		if err != nil {
			return nil, fmt.Errorf("failed to create agent database: %w", err)
		}
	}

	// 3. 创建连接到agent数据库的客户端
	agentClient, err := cli.NewClient(ctx, cli.Config{
		Address: address,
		DBName:  knowledge.MilvusDBName,
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

	// 4. 检查biz collection是否存在，不存在则创建
	collections, err := agentClient.ListCollections(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to list collections: %w", err)
	}

	bizCollectionExists := false
	for _, collection := range collections {
		if collection.Name == knowledge.MilvusCollectionName {
			bizCollectionExists = true
			break
		}
	}

	if !bizCollectionExists {
		// 创建biz collection的schema
		schema := &entity.Schema{
			CollectionName: knowledge.MilvusCollectionName,
			Description:    "Business knowledge collection",
			Fields:         fields,
		}

		err = agentClient.CreateCollection(ctx, schema, entity.DefaultShardNumber)
		if err != nil {
			return nil, fmt.Errorf("failed to create biz collection: %w", err)
		}

		// 为id字段创建autoindex索引
		idIndex, err := entity.NewIndexAUTOINDEX(entity.L2)
		if err != nil {
			return nil, fmt.Errorf("failed to create id index: %w", err)
		}
		err = agentClient.CreateIndex(ctx, knowledge.MilvusCollectionName, "id", idIndex, false)
		if err != nil {
			return nil, fmt.Errorf("failed to create id index: %w", err)
		}

		// 为content字段创建autoindex索引
		contentIndex, err := entity.NewIndexAUTOINDEX(entity.L2)
		if err != nil {
			return nil, fmt.Errorf("failed to create content index: %w", err)
		}
		err = agentClient.CreateIndex(ctx, knowledge.MilvusCollectionName, "content", contentIndex, false)
		if err != nil {
			return nil, fmt.Errorf("failed to create content index: %w", err)
		}

		// 为vector字段创建autoindex索引
		vectorIndex, err := entity.NewIndexAUTOINDEX(entity.HAMMING)
		if err != nil {
			return nil, fmt.Errorf("failed to create vector index: %w", err)
		}
		err = agentClient.CreateIndex(ctx, knowledge.MilvusCollectionName, "vector", vectorIndex, false)
		if err != nil {
			return nil, fmt.Errorf("failed to create vector index: %w", err)
		}
	}

	if err := ensureCollectionLoaded(ctx, agentClient); err != nil {
		return nil, err
	}

	agentClientReady = true
	return agentClient, nil
}

// 确保collection处于加载状态，如果正在加载则等待加载完成，如果未加载则触发加载
func ensureCollectionLoaded(ctx context.Context, agentClient cli.Client) error {
	state, err := agentClient.GetLoadState(ctx, knowledge.MilvusCollectionName, nil)
	if err != nil {
		return fmt.Errorf("failed to get biz collection load state: %w", err)
	}

	switch state {
	case entity.LoadStateLoaded:
		return nil
	case entity.LoadStateLoading:
		return waitCollectionLoaded(ctx, agentClient)
	default:
		if err := agentClient.LoadCollection(ctx, knowledge.MilvusCollectionName, false); err != nil {
			return fmt.Errorf("failed to load biz collection: %w", err)
		}
		return nil
	}
}

// 轮询检查collection是否加载完成，直到加载完成或上下文超时
func waitCollectionLoaded(ctx context.Context, agentClient cli.Client) error {
	ticker := time.NewTicker(200 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			state, err := agentClient.GetLoadState(ctx, knowledge.MilvusCollectionName, nil)
			if err != nil {
				return fmt.Errorf("failed to get biz collection load state: %w", err)
			}
			if state == entity.LoadStateLoaded {
				return nil
			}
			if state != entity.LoadStateLoading {
				return fmt.Errorf("biz collection load stopped with state %d", state)
			}
		}
	}
}

// 定义collection schema的字段
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
