package main

import (
	"fmt"
	"log"

	"monitor-system/api_gateway/internal/config"
	"monitor-system/api_gateway/internal/grpcclient"
	"monitor-system/api_gateway/internal/handler"
	"monitor-system/api_gateway/internal/logger"
)

func main() {
	cfg := config.Load()
	logger.Init(cfg.Mode)

	// 初始化 gRPC 客户端
	queryClient, err := grpcclient.New(cfg.ManagerAddr, cfg.ManagerTimeout)
	if err != nil {
		log.Fatalf("failed to initialize manager grpc client: %v", err)
	}
	defer queryClient.Close()

	// 设置 HTTP 路由
	router := handler.NewRouter(cfg, queryClient)
	addr := fmt.Sprintf(":%s", cfg.Port)

	log.Printf("api_gateway listening on %s", addr)
	if err := router.Run(addr); err != nil {
		log.Fatalf("api_gateway stopped: %v", err)
	}
}
