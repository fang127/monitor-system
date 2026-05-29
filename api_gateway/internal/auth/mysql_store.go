package auth

import (
	"context"
	"errors"
	"strings"

	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"
)

type MySQLUserStore struct {
	db *gorm.DB
}

// NewMySQLUserStore 创建一个新的 MySQL 用户存储实例
func NewMySQLUserStore(db *gorm.DB) *MySQLUserStore {
	return &MySQLUserStore{db: db}
}

// EnsureSchema 确保数据库中存在用户表
func (s *MySQLUserStore) EnsureSchema(ctx context.Context) error {
	return s.db.WithContext(ctx).AutoMigrate(&User{})
}

// BootstrapAdminIfEmpty 如果用户表为空，则创建一个管理员账户
func (s *MySQLUserStore) BootstrapAdminIfEmpty(ctx context.Context, username string, password string) error {
	var count int64
	if err := s.db.WithContext(ctx).Model(&User{}).Count(&count).Error; err != nil {
		return err
	}
	if count > 0 {
		return nil
	}
	username = strings.TrimSpace(username)
	if username == "" || password == "" {
		return errors.New("用户表为空时必须配置 ADMIN_USERNAME 和 ADMIN_PASSWORD")
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return err
	}
	return s.db.WithContext(ctx).Create(&User{
		Username:     username,
		PasswordHash: string(hash),
		Role:         RoleAdmin,
		Status:       StatusActive,
	}).Error
}

// FindByUsername 根据用户名查找用户
func (s *MySQLUserStore) FindByUsername(username string) (User, error) {
	var user User
	err := s.db.WithContext(context.Background()).Where("username = ?", username).First(&user).Error
	if errors.Is(err, gorm.ErrRecordNotFound) {
		return User{}, ErrInvalidCredentials
	}
	return user, err
}

// Create 创建一个新用户
func (s *MySQLUserStore) Create(username string, passwordHash string, role Role) (User, error) {
	user := User{
		Username:     username,
		PasswordHash: passwordHash,
		Role:         role,
		Status:       StatusActive,
	}
	err := s.db.WithContext(context.Background()).Create(&user).Error
	if err != nil {
		if errors.Is(err, gorm.ErrDuplicatedKey) {
			return User{}, ErrDuplicateUser
		}
		return User{}, err
	}
	return user, nil
}
