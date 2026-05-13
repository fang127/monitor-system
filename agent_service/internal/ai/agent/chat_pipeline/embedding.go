package chat_pipeline

import (
	"context"
	"monitor-system/agent_service/internal/ai/embedder"

	"github.com/cloudwego/eino/components/embedding"
)

func newEmbedding(ctx context.Context) (eb embedding.Embedder, err error) {
	return embedder.CrearteEmbedding(ctx)
}
