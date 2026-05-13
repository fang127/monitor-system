package main

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/controller/chat"
	"monitor-system/agent_service/utility/common"
	"monitor-system/agent_service/utility/middleware"

	"github.com/gin-gonic/gin"
)

func main() {
	ctx := context.Background()
	fileDir, err := common.ConfigString(ctx, "docs_dir", "AGENT_DOCS_DIR", "./docs")
	if err != nil {
		panic(err)
	}
	common.FileDir = fileDir
	port, err := common.ConfigInt(ctx, "agent_service_port", "AGENT_SERVICE_PORT", 6872)
	if err != nil {
		panic(err)
	}
	router := gin.New()
	router.Use(gin.Logger(), gin.Recovery(), middleware.CORSMiddleware())
	controller := chat.NewV1()
	controller.RegisterRoutes(router.Group("/api/agent"))
	if err := router.Run(fmt.Sprintf(":%d", port)); err != nil {
		panic(err)
	}
}
