package retriever

import (
	"context"
	"monitor-system/agent_service/internal/ai/embedder"
	"monitor-system/agent_service/utility/client"
	"monitor-system/agent_service/utility/common"
	"strings"

	"github.com/cloudwego/eino-ext/components/retriever/milvus"
	einoRetriever "github.com/cloudwego/eino/components/retriever"
	"github.com/cloudwego/eino/schema"
)

// NewMilvusRetriever创建一个基于Milvus的Retriever实例，从而实现向量数据库的检索功能。
func NewMilvusRetriever(ctx context.Context) (rtr einoRetriever.Retriever, err error) {
	// 创建Milvus客户端
	cli, err := client.NewMilvusClient(ctx)
	if err != nil {
		return nil, err
	}
	// 获取DoubaoEmbedding实例
	eb, err := embedder.CrearteEmbedding(ctx)
	if err != nil {
		return nil, err
	}
	r, err := milvus.NewRetriever(ctx, &milvus.RetrieverConfig{
		Client:      cli,
		Collection:  common.MilvusCollectionName,
		VectorField: "vector",
		OutputFields: []string{
			"id",
			"content",
			"metadata",
		},
		TopK:      1,
		Embedding: eb,
	})
	if err != nil {
		return nil, err
	}
	return tolerantRetriever{base: r}, nil
}

type tolerantRetriever struct {
	base einoRetriever.Retriever
}

func (r tolerantRetriever) Retrieve(ctx context.Context, query string, opts ...einoRetriever.Option) ([]*schema.Document, error) {
	docs, err := r.base.Retrieve(ctx, query, opts...)
	if err == nil {
		return docs, nil
	}
	if isEmptyMilvusResultError(err) {
		return []*schema.Document{}, nil
	}
	return nil, err
}

func isEmptyMilvusResultError(err error) bool {
	message := err.Error()
	return strings.Contains(message, "no results found") ||
		strings.Contains(message, "extra output fields") ||
		strings.Contains(message, "result does not dynamic field")
}
