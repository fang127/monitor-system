package main

import (
	"context"
	"fmt"
	"log"

	"monitor-system/api_gateway/internal/auth"
	"monitor-system/api_gateway/internal/config"
	"monitor-system/api_gateway/internal/grpcclient"
	"monitor-system/api_gateway/internal/handler"
	"monitor-system/api_gateway/internal/logger"

	"gorm.io/driver/mysql"
	"gorm.io/gorm"
)

func main() {
	cfg := config.Load()
	logger.Init(cfg.Mode)
	ctx := context.Background()

	// 初始化 MySQL 连接
	db, err := gorm.Open(mysql.Open(cfg.MySQLDSN()), &gorm.Config{TranslateError: true})
	if err != nil {
		log.Fatalf("failed to initialize mysql connection: %v", err)
	}
	// 确保 MySQL 连接可用
	sqlDB, err := db.DB()
	if err != nil {
		log.Fatalf("failed to get mysql connection pool: %v", err)
	}
	defer sqlDB.Close()
	if err := sqlDB.PingContext(ctx); err != nil {
		log.Fatalf("failed to connect mysql: %v", err)
	}
	// 初始化用户存储并确保管理员账户存在
	userStore := auth.NewMySQLUserStore(db)
	if err := userStore.EnsureSchema(ctx); err != nil {
		log.Fatalf("failed to ensure users schema: %v", err)
	}
	// 如果用户表为空，则创建一个默认管理员账户
	if err := userStore.BootstrapAdminIfEmpty(ctx, cfg.AdminUsername, cfg.AdminPassword); err != nil {
		log.Fatalf("failed to bootstrap admin user: %v", err)
	}

	// 初始化 gRPC 客户端
	queryClient, err := grpcclient.New(cfg.ManagerAddr, cfg.ManagerTimeout)
	if err != nil {
		log.Fatalf("failed to initialize manager grpc client: %v", err)
	}
	defer queryClient.Close()

	// 设置 HTTP 路由
	router := handler.NewRouter(cfg, queryClient, handler.RouterOptions{UserStore: userStore})
	addr := fmt.Sprintf(":%s", cfg.Port)

	log.Printf("api_gateway listening on %s", addr)
	if err := router.Run(addr); err != nil {
		log.Fatalf("api_gateway stopped: %v", err)
	}
}
