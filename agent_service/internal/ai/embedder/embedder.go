package embedder

import (
	"context"
	"log"
	"monitor-system/agent_service/internal/config"

	"github.com/cloudwego/eino-ext/components/embedding/dashscope"
	"github.com/cloudwego/eino/components/embedding"
)

// CrearteEmbedding 创建一个新的 Embedder 实例，使用配置中的模型名称和 API 密钥。
func CrearteEmbedding(ctx context.Context) (eb embedding.Embedder, err error) {
	modelName, err := config.ConfigString(ctx, "embedding_model.model", "AGENT_EMBEDDING_MODEL", "text-embedding-v4")
	if err != nil {
		return nil, err
	}
	apiKey, err := config.ConfigString(ctx, "embedding_model.api_key", "AGENT_EMBEDDING_API_KEY", "")
	if err != nil {
		return nil, err
	}
	dim := 2048
	embedder, err := dashscope.NewEmbedder(ctx, &dashscope.EmbeddingConfig{
		Model:      modelName,
		APIKey:     apiKey,
		Dimensions: &dim,
	})
	if err != nil {
		log.Printf("new embedder error: %v\n", err)
		return nil, err
	}
	return embedder, nil
}
