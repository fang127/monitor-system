package main

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/config"
	"monitor-system/agent_service/internal/handler/agent"
	"monitor-system/agent_service/internal/response"
	"monitor-system/agent_service/internal/storage/knowledge"

	"github.com/gin-gonic/gin"
)

func main() {
	ctx := context.Background()
	// 读取配置项
	fileDir, err := config.ConfigString(ctx, "docs_dir", "AGENT_DOCS_DIR", "./docs")
	if err != nil {
		panic(err)
	}
	knowledge.FileDir = fileDir
	// 读取端口配置项
	port, err := config.ConfigInt(ctx, "agent_service_port", "AGENT_SERVICE_PORT", 6872)
	if err != nil {
		panic(err)
	}
	// 启动HTTP服务器
	router := gin.New()
	router.Use(gin.Logger(), gin.Recovery(), response.CORSMiddleware())
	controller := agent.NewV1()
	controller.RegisterRoutes(router.Group("/api/agent"))
	if err := router.Run(fmt.Sprintf(":%d", port)); err != nil {
		panic(err)
	}
}
