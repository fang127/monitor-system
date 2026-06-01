package mysql

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/config"
	"time"

	gormmysql "gorm.io/driver/mysql"
	"gorm.io/gorm"
)

// Config 描述 agent_service 自己的 MySQL 连接配置，不复用 manager 连接池。
type Config struct {
	Enabled         bool
	Host            string
	Port            int
	User            string
	Password        string
	Database        string
	MaxOpenConns    int
	MaxIdleConns    int
	ConnMaxLifetime time.Duration
}

// LoadConfig 读取 agent_service 独立 MySQL 配置。
func LoadConfig(ctx context.Context) (Config, error) {
	cfg := Config{Host: "127.0.0.1", Port: 3306, User: "root", Database: "monitor-system", MaxOpenConns: 8, MaxIdleConns: 2, ConnMaxLifetime: time.Hour}
	var err error
	if cfg.Enabled, err = config.ConfigBool(ctx, "agent_mysql.enabled", "AGENT_MYSQL_ENABLED", cfg.Enabled); err != nil {
		return cfg, err
	}
	if cfg.Host, err = config.ConfigString(ctx, "agent_mysql.host", "AGENT_MYSQL_HOST", cfg.Host); err != nil {
		return cfg, err
	}
	if cfg.Port, err = config.ConfigInt(ctx, "agent_mysql.port", "AGENT_MYSQL_PORT", cfg.Port); err != nil {
		return cfg, err
	}
	if cfg.User, err = config.ConfigString(ctx, "agent_mysql.user", "AGENT_MYSQL_USER", cfg.User); err != nil {
		return cfg, err
	}
	if cfg.Password, err = config.ConfigString(ctx, "agent_mysql.password", "AGENT_MYSQL_PASSWORD", cfg.Password); err != nil {
		return cfg, err
	}
	if cfg.Database, err = config.ConfigString(ctx, "agent_mysql.database", "AGENT_MYSQL_DATABASE", cfg.Database); err != nil {
		return cfg, err
	}
	if cfg.MaxOpenConns, err = config.ConfigInt(ctx, "agent_mysql.max_open_conns", "AGENT_MYSQL_MAX_OPEN_CONNS", cfg.MaxOpenConns); err != nil {
		return cfg, err
	}
	if cfg.MaxIdleConns, err = config.ConfigInt(ctx, "agent_mysql.max_idle_conns", "AGENT_MYSQL_MAX_IDLE_CONNS", cfg.MaxIdleConns); err != nil {
		return cfg, err
	}
	lifetimeSeconds, err := config.ConfigInt(ctx, "agent_mysql.conn_max_lifetime_seconds", "AGENT_MYSQL_CONN_MAX_LIFETIME_SECONDS", int(cfg.ConnMaxLifetime.Seconds()))
	if err != nil {
		return cfg, err
	}
	cfg.ConnMaxLifetime = time.Duration(lifetimeSeconds) * time.Second
	return cfg, nil
}

// DSN 返回 GORM MySQL driver 使用的连接串。
func (c Config) MysqlDSN() string {
	return fmt.Sprintf("%s:%s@tcp(%s:%d)/%s?parseTime=true&charset=utf8mb4", c.User, c.Password, c.Host, c.Port, c.Database)
}

// Open 创建 agent_service 自己的 GORM MySQL 连接。
func Open(ctx context.Context) (*gorm.DB, Config, error) {
	cfg, err := LoadConfig(ctx)
	if err != nil {
		return nil, cfg, err
	}
	if !cfg.Enabled {
		return nil, cfg, nil
	}
	db, err := gorm.Open(gormmysql.Open(cfg.MysqlDSN()), &gorm.Config{TranslateError: true})
	if err != nil {
		return nil, cfg, err
	}
	sqlDB, err := db.DB()
	if err != nil {
		return nil, cfg, err
	}
	sqlDB.SetMaxOpenConns(cfg.MaxOpenConns)
	sqlDB.SetMaxIdleConns(cfg.MaxIdleConns)
	sqlDB.SetConnMaxLifetime(cfg.ConnMaxLifetime)
	if err := sqlDB.PingContext(ctx); err != nil {
		_ = sqlDB.Close()
		return nil, cfg, err
	}
	return db, cfg, nil
}
