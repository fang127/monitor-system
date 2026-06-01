package memory

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/storage/milvus_config"
	"strconv"
	"strings"

	einoMilvusIndexer "github.com/cloudwego/eino-ext/components/indexer/milvus"
	einoMilvusRetriever "github.com/cloudwego/eino-ext/components/retriever/milvus"
	"github.com/cloudwego/eino/components/embedding"
	einoIndexer "github.com/cloudwego/eino/components/indexer"
	einoRetriever "github.com/cloudwego/eino/components/retriever"
	"github.com/cloudwego/eino/schema"
	cli "github.com/milvus-io/milvus-sdk-go/v2/client"
	"github.com/milvus-io/milvus-sdk-go/v2/entity"
)

// LongTermMemoryCollectionName 是长期记忆专用 Milvus collection 名称，必须和 ops_docs 隔离。
const LongTermMemoryCollectionName = milvus_config.LongTermMemoryCollection

// MilvusLongTermVectorIndex 是长期记忆专用向量索引的生产实现。
type MilvusLongTermVectorIndex struct {
	collection string
	client     cli.Client              // Milvus 客户端实例
	indexer    einoIndexer.Indexer     // indexer 组件负责向 Milvus 写入向量数据
	retriever  einoRetriever.Retriever // retriever 组件负责从 Milvus 搜索向量数据
}

// NewMilvusLongTermVectorIndex 创建长期记忆专用 Milvus 索引。
func NewMilvusLongTermVectorIndex(ctx context.Context, client cli.Client, embedder embedding.Embedder) (*MilvusLongTermVectorIndex, error) {
	if client == nil {
		return nil, fmt.Errorf("Milvus 客户端未配置")
	}
	if embedder == nil {
		return nil, fmt.Errorf("长期记忆 embedding 未配置")
	}
	indexer, err := einoMilvusIndexer.NewIndexer(ctx, &einoMilvusIndexer.IndexerConfig{
		Client:      client,
		Collection:  LongTermMemoryCollectionName,
		Description: "Agent long term memory collection",
		Fields:      longTermMemoryMilvusFields(),
		Embedding:   embedder,
	})
	if err != nil {
		return nil, err
	}
	retriever, err := einoMilvusRetriever.NewRetriever(ctx, &einoMilvusRetriever.RetrieverConfig{
		Client:       client,
		Collection:   LongTermMemoryCollectionName,
		VectorField:  "vector",
		OutputFields: []string{"id", "content", "metadata"},
		TopK:         DefaultConfig().RecallLimit,
		Embedding:    embedder,
	})
	if err != nil {
		return nil, err
	}
	return &MilvusLongTermVectorIndex{
		collection: LongTermMemoryCollectionName,
		client:     client,
		indexer:    indexer,
		retriever:  retriever,
	}, nil
}

// Collection 返回当前索引使用的 Milvus collection。
func (i *MilvusLongTermVectorIndex) Collection() string {
	if i == nil || i.collection == "" {
		return LongTermMemoryCollectionName
	}
	return i.collection
}

// Upsert 写入长期记忆向量。
func (i *MilvusLongTermVectorIndex) Upsert(ctx context.Context, item LongTermMemory) error {
	if i == nil || i.indexer == nil {
		return fmt.Errorf("Milvus 长期记忆向量索引未配置")
	}
	if strings.TrimSpace(item.ID) == "" {
		return fmt.Errorf("长期记忆 ID 不能为空")
	}
	// 先删除旧数据，再写入新数据，确保 ID 唯一且内容更新。Milvus SDK 不支持直接覆盖写入。
	if err := i.Delete(ctx, []string{item.ID}); err != nil {
		return err
	}
	_, err := i.indexer.Store(ctx, []*schema.Document{{
		ID:      item.ID,
		Content: item.Content,
		MetaData: map[string]any{
			"memory_id":    item.ID,
			"tenant_id":    item.Scope.TenantID,
			"team_id":      item.Scope.TeamID,
			"cluster_id":   item.Scope.ClusterID,
			"type":         item.Type,
			"scope_level":  item.ScopeLevel,
			"status":       item.Status,
			"confidence":   item.Confidence,
			"sensitivity":  item.Sensitivity,
			"content_hash": item.ContentHash,
		},
	}})
	return err
}

// Search 搜索长期记忆向量候选。
func (i *MilvusLongTermVectorIndex) Search(ctx context.Context, selector MemorySelector) ([]VectorSearchResult, error) {
	if i == nil || i.retriever == nil {
		return nil, fmt.Errorf("Milvus 长期记忆向量索引未配置")
	}
	query := strings.TrimSpace(selector.Query)
	if query == "" {
		return []VectorSearchResult{}, nil
	}
	limit := selector.Limit
	if limit <= 0 {
		limit = DefaultConfig().RecallLimit
	}
	docs, err := i.retriever.Retrieve(ctx, query, einoRetriever.WithTopK(limit))
	if err != nil {
		if isEmptyMilvusSearchError(err) {
			return []VectorSearchResult{}, nil
		}
		return nil, err
	}
	results := make([]VectorSearchResult, 0, len(docs))
	for _, doc := range docs {
		if doc == nil || doc.ID == "" {
			continue
		}
		score := normalizeMilvusScore(doc.Score())
		if selector.MinScore > 0 && score < selector.MinScore {
			continue
		}
		results = append(results, VectorSearchResult{ID: doc.ID, Score: score})
	}
	return results, nil
}

// Delete 删除长期记忆向量。
func (i *MilvusLongTermVectorIndex) Delete(ctx context.Context, ids []string) error {
	if i == nil || i.client == nil {
		return fmt.Errorf("Milvus 长期记忆向量索引未配置")
	}
	quotedIDs := make([]string, 0, len(ids))
	for _, id := range ids {
		id = strings.TrimSpace(id)
		if id == "" {
			continue
		}
		quotedIDs = append(quotedIDs, strconv.Quote(id))
	}
	if len(quotedIDs) == 0 {
		return nil
	}
	return i.client.Delete(ctx, i.Collection(), "", "id in ["+strings.Join(quotedIDs, ",")+"]")
}

func longTermMemoryMilvusFields() []*entity.Field {
	return []*entity.Field{
		{
			Name:       "id",
			DataType:   entity.FieldTypeVarChar,
			TypeParams: map[string]string{"max_length": "256"},
			PrimaryKey: true,
		},
		{
			Name:       "vector",
			DataType:   entity.FieldTypeBinaryVector,
			TypeParams: map[string]string{"dim": "65536"},
		},
		{
			Name:       "content",
			DataType:   entity.FieldTypeVarChar,
			TypeParams: map[string]string{"max_length": "8192"},
		},
		{
			Name:     "metadata",
			DataType: entity.FieldTypeJSON,
		},
	}
}

func normalizeMilvusScore(score float64) float64 {
	if score <= 0 {
		return 1
	}
	return 1 / (1 + score)
}

func isEmptyMilvusSearchError(err error) bool {
	if err == nil {
		return false
	}
	message := err.Error()
	return strings.Contains(message, "no results found") ||
		strings.Contains(message, "extra output fields") ||
		strings.Contains(message, "result does not dynamic field")
}
