package retriever

import (
	"context"
	"monitor-system/agent_service/internal/ai/embedder"
	storageMilvus "monitor-system/agent_service/internal/storage/milvus"
	"monitor-system/agent_service/internal/storage/milvus_config"
	"strings"

	einoMilvus "github.com/cloudwego/eino-ext/components/retriever/milvus"
	einoRetriever "github.com/cloudwego/eino/components/retriever"
	"github.com/cloudwego/eino/schema"
)

// NewMilvusRetriever创建一个基于Milvus的Retriever实例，从而实现向量数据库的检索功能。
func NewMilvusRetriever(ctx context.Context) (rtr einoRetriever.Retriever, err error) {
	// 创建Milvus客户端
	cli, err := storageMilvus.NewMilvusClient(ctx)
	if err != nil {
		return nil, err
	}
	// 获取Embedding实例
	eb, err := embedder.CrearteEmbedding(ctx)
	if err != nil {
		return nil, err
	}
	r, err := einoMilvus.NewRetriever(ctx, &einoMilvus.RetrieverConfig{
		Client:      cli,
		Collection:  milvus_config.AgentOpsDocsCollection,
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

// Retrieve方法实现了对Milvus检索结果的容错处理。当检索过程中发生错误时，如果错误信息表明是由于Milvus没有找到匹配的结果导致的，那么该方法将返回一个空的文档列表，而不是将错误直接返回给调用者。这种设计允许调用者在没有匹配结果的情况下继续正常工作，而不会因为错误而中断流程。
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

// isEmptyMilvusResultError函数用于判断一个错误是否是由于Milvus没有找到匹配结果而引起的。它通过检查错误消息中是否包含特定的字符串来进行判断。这些字符串包括"no results found"（没有找到结果）、"extra output fields"（额外的输出字段）和"result does not dynamic field"（结果没有动态字段）。如果错误消息中包含这些字符串中的任何一个，那么该函数将返回true，表示这是一个空结果错误；否则，它将返回false。
func isEmptyMilvusResultError(err error) bool {
	message := err.Error()
	return strings.Contains(message, "no results found") ||
		strings.Contains(message, "extra output fields") ||
		strings.Contains(message, "result does not dynamic field")
}
