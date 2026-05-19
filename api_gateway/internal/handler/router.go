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
	queryHandler := NewQueryHandler(queryClient, cfg.ManagerAddr)

	router.GET("/health", healthHandler.Get)
	router.GET("/api/version", versionHandler.Get)
	router.GET("/api/servers/latest", queryHandler.Latest)
	router.GET("/api/servers/score-rank", queryHandler.ScoreRank)
	router.GET("/api/servers/:server/performance", queryHandler.Performance)
	router.GET("/api/servers/:server/trend", queryHandler.Trend)
	router.GET("/api/servers/:server/anomalies", queryHandler.Anomalies)
	router.GET("/api/servers/:server/net-detail", queryHandler.NetDetail)
	router.GET("/api/servers/:server/disk-detail", queryHandler.DiskDetail)
	router.GET("/api/servers/:server/mem-detail", queryHandler.MemDetail)
	router.GET("/api/servers/:server/softirq-detail", queryHandler.SoftIrqDetail)
	router.GET("/api/servers/:server/mysql-detail", queryHandler.MysqlDetail)
	router.GET("/api/servers/:server/redis-detail", queryHandler.RedisDetail)

	return router
}
