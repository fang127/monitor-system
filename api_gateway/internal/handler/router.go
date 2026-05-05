package handler

import (
	"github.com/gin-gonic/gin"

	"monitor-system/api_gateway/internal/config"
	"monitor-system/api_gateway/internal/grpcclient"
)

func NewRouter(cfg config.Config, queryClient *grpcclient.Client) *gin.Engine {
	gin.SetMode(cfg.Mode)

	router := gin.New()
	router.Use(gin.Logger(), gin.Recovery())

	healthHandler := NewHealthHandler()
	versionHandler := NewVersionHandler(cfg.Version)
	queryHandler := NewQueryHandler(queryClient)

	router.GET("/health", healthHandler.Get)
	router.GET("/api/version", versionHandler.Get)
	router.GET("/api/servers/latest", queryHandler.Latest)
	router.GET("/api/servers/:server/trend", queryHandler.Trend)
	router.GET("/api/servers/:server/anomalies", queryHandler.Anomalies)

	return router
}
