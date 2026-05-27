package main

import (
	"context"
	"fmt"
	"log"

	"monitor-system/agent_service/internal/agent"
	"monitor-system/agent_service/internal/config"
	"monitor-system/agent_service/internal/gateway"
	"monitor-system/agent_service/internal/httpapi"
	"monitor-system/agent_service/internal/knowledge"
	"monitor-system/agent_service/internal/memory"
	"monitor-system/agent_service/internal/model"
)

func main() {
	cfg, err := config.Load("")
	if err != nil {
		log.Fatalf("load config: %v", err)
	}

	ctx := context.Background()
	gatewayClient := gateway.NewClient(cfg.APIGatewayBaseURL, nil)
	var knowledgeStore knowledge.Store = knowledge.NewLocalStore(cfg.DocsDir)
	memStore := memory.NewStore(cfg.MemoryEnabled, 6)
	quickModel := model.NewOpenAICompatible(model.Config{
		APIKey:  cfg.QuickModel.APIKey,
		BaseURL: cfg.QuickModel.BaseURL,
		Model:   cfg.QuickModel.Model,
	})
	thinkModel := model.NewOpenAICompatible(model.Config{
		APIKey:  cfg.ThinkModel.APIKey,
		BaseURL: cfg.ThinkModel.BaseURL,
		Model:   cfg.ThinkModel.Model,
	})
	quickEino, err := model.NewEinoOpenAIModel(ctx, model.Config{
		APIKey:  cfg.QuickModel.APIKey,
		BaseURL: cfg.QuickModel.BaseURL,
		Model:   cfg.QuickModel.Model,
	})
	if err != nil {
		log.Printf("agent_service degraded: quick Eino model unavailable: %v", err)
	}
	thinkEino, err := model.NewEinoOpenAIModel(ctx, model.Config{
		APIKey:  cfg.ThinkModel.APIKey,
		BaseURL: cfg.ThinkModel.BaseURL,
		Model:   cfg.ThinkModel.Model,
	})
	if err != nil {
		log.Printf("agent_service degraded: think Eino model unavailable: %v", err)
	}
	embedder, err := model.NewDashScopeEmbedder(ctx, model.EmbeddingConfig{
		APIKey:     cfg.EmbeddingAPIKey,
		Model:      cfg.EmbeddingModel,
		Dimensions: cfg.EmbeddingDim,
	})
	if err != nil {
		log.Printf("agent_service degraded: embedding unavailable, using local lexical docs store: %v", err)
	} else {
		milvusStore, err := knowledge.NewMilvusStore(ctx, knowledge.MilvusConfig{
			Address:    cfg.MilvusAddr,
			Database:   knowledge.DefaultMilvusDatabase,
			Collection: knowledge.DefaultMilvusCollection,
			DocsDir:    cfg.DocsDir,
			Dimension:  cfg.EmbeddingDim,
			TopK:       3,
		}, embedder)
		if err != nil {
			log.Printf("agent_service degraded: Milvus unavailable, using local lexical docs store: %v", err)
		} else {
			knowledgeStore = milvusStore
			defer func() {
				if err := milvusStore.Close(); err != nil {
					log.Printf("close Milvus client: %v", err)
				}
			}()
		}
	}

	agentSvc := agent.NewServiceWithEino(gatewayClient, knowledgeStore, quickModel, thinkModel, memStore, quickEino, thinkEino)
	router := httpapi.NewRouterWithOptions(agentSvc, httpapi.Options{DocsDir: cfg.DocsDir})
	addr := fmt.Sprintf(":%d", cfg.Port)
	log.Printf("agent_service listening on %s", addr)
	if err := router.Run(addr); err != nil {
		log.Fatalf("run server: %v", err)
	}
}
