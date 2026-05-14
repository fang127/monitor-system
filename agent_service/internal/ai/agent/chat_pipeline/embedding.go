package chat_pipeline

import (
	"context"
	"monitor-system/agent_service/internal/ai/embedder"

	"github.com/cloudwego/eino/components/embedding"
)

// newEmbedding 创建一个新的 Embedder 实例
func newEmbedding(ctx context.Context) (eb embedding.Embedder, err error) {
	return embedder.CrearteEmbedding(ctx)
}
