package handler

import (
	"github.com/gin-gonic/gin"

	"monitor-system/api_gateway/internal/auth"
	"monitor-system/api_gateway/internal/config"
	"monitor-system/api_gateway/internal/grpcclient"
)

// API路由配置
type RouterOptions struct {
	UserStore auth.UserStore
}

func NewRouter(cfg config.Config, queryClient *grpcclient.Client, opts ...RouterOptions) *gin.Engine {
	gin.SetMode(cfg.Mode)

	router := gin.New()
	router.Use(gin.Logger(), gin.Recovery())

	healthHandler := NewHealthHandler()
	versionHandler := NewVersionHandler(cfg.Version)
	queryHandler := NewQueryHandler(queryClient, cfg.ManagerAddr)
	userStore := auth.UserStore(nil) // 默认使用nil，如果提供了UserStore，则覆盖默认值
	if len(opts) > 0 {
		userStore = opts[0].UserStore
	}
	// 创建token管理器
	tokenManager := auth.NewTokenManager(cfg.JWTSecret, cfg.JWTAccessTTL)
	// 创建认证服务
	authService := auth.NewService(userStore, tokenManager)
	// 创建认证处理器
	authHandler := NewAuthHandler(authService)

	// 健康检查和版本信息接口不需要认证
	router.GET("/health", healthHandler.Get)
	router.GET("/api/version", versionHandler.Get)
	router.POST("/api/auth/login", authHandler.Login)

	// 需要认证的API路由
	api := router.Group("/api")                                                  // 创建一个/api前缀的路由组
	api.Use(auth.Middleware(tokenManager))                                       // 应用认证中间件
	api.GET("/auth/me", authHandler.Me)                                          // 获取当前用户信息
	api.POST("/users", auth.RequireRole(auth.RoleAdmin), authHandler.CreateUser) // 只有管理员可以创建用户

	api.GET("/servers/latest", queryHandler.Latest)
	api.GET("/servers/score-rank", queryHandler.ScoreRank)
	api.GET("/servers/:server/performance", queryHandler.Performance)
	api.GET("/servers/:server/trend", queryHandler.Trend)
	api.GET("/servers/:server/anomalies", queryHandler.Anomalies)
	api.GET("/servers/:server/net-detail", queryHandler.NetDetail)
	api.GET("/servers/:server/disk-detail", queryHandler.DiskDetail)
	api.GET("/servers/:server/mem-detail", queryHandler.MemDetail)
	api.GET("/servers/:server/softirq-detail", queryHandler.SoftIrqDetail)
	api.GET("/servers/:server/mysql-detail", queryHandler.MysqlDetail)
	api.GET("/servers/:server/redis-detail", queryHandler.RedisDetail)

	return router
}
