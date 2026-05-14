package chat_pipeline

import (
	"context"
	retriever2 "monitor-system/agent_service/internal/ai/retriever"

	"github.com/cloudwego/eino/components/retriever"
)

// newRetriever 创建一个新的检索器实例
func newRetriever(ctx context.Context) (rtr retriever.Retriever, err error) {
	return retriever2.NewMilvusRetriever(ctx)
}
