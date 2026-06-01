package memory

import (
	"context"
	"fmt"
	storageMemory "monitor-system/agent_service/internal/storage/memory"
)

// LongTermMemoryCollectionName 是长期记忆专用 Milvus collection 名称，必须和 ops_docs 隔离。
const LongTermMemoryCollectionName = storageMemory.LongTermMemoryCollection

// MilvusLongTermVectorIndex 是长期记忆专用向量索引的生产边界。
// 当前类型通过 delegate 适配实际 Milvus 实现，确保上层永远面向长期记忆专用 collection。
type MilvusLongTermVectorIndex struct {
	collection string
	delegate   VectorIndex
}

// NewMilvusLongTermVectorIndex 创建长期记忆专用 Milvus 索引。
func NewMilvusLongTermVectorIndex(delegate VectorIndex) *MilvusLongTermVectorIndex {
	return &MilvusLongTermVectorIndex{collection: LongTermMemoryCollectionName, delegate: delegate}
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
	if i == nil || i.delegate == nil {
		return fmt.Errorf("Milvus 长期记忆向量索引未配置")
	}
	return i.delegate.Upsert(ctx, item)
}

// Search 搜索长期记忆向量候选。
func (i *MilvusLongTermVectorIndex) Search(ctx context.Context, selector MemorySelector) ([]VectorSearchResult, error) {
	if i == nil || i.delegate == nil {
		return nil, fmt.Errorf("Milvus 长期记忆向量索引未配置")
	}
	return i.delegate.Search(ctx, selector)
}

// Delete 删除长期记忆向量。
func (i *MilvusLongTermVectorIndex) Delete(ctx context.Context, ids []string) error {
	if i == nil || i.delegate == nil {
		return fmt.Errorf("Milvus 长期记忆向量索引未配置")
	}
	return i.delegate.Delete(ctx, ids)
}
