package auth

import (
	"errors"
	"strings"
	"time"

	"golang.org/x/crypto/bcrypt"
)

type UserStore interface {
	FindByUsername(username string) (User, error)
	Create(username string, passwordHash string, role Role) (User, error)
}

// 给前端返回的用户信息，不包含敏感字段
type LoginResult struct {
	AccessToken string    `json:"access_token"`
	ExpiresAt   time.Time `json:"expires_at"`
	User        User      `json:"user"`
}

type Service struct {
	store        UserStore
	tokenManager *TokenManager
}

// NewService 创建一个新的认证服务实例
func NewService(store UserStore, tokenManager *TokenManager) *Service {
	return &Service{
		store:        store,
		tokenManager: tokenManager,
	}
}

// Login 验证用户凭据并生成访问令牌
func (s *Service) Login(username string, password string) (LoginResult, error) {
	if s == nil || s.store == nil || s.tokenManager == nil {
		return LoginResult{}, errors.New("认证服务未配置")
	}
	username = strings.TrimSpace(username)
	if username == "" || password == "" {
		return LoginResult{}, ErrInvalidCredentials
	}
	// 从存储中查找用户
	user, err := s.store.FindByUsername(username)
	if err != nil {
		return LoginResult{}, ErrInvalidCredentials
	}
	if user.Status != StatusActive {
		return LoginResult{}, ErrUserDisabled
	}
	// bcrypt.CompareHashAndPassword 返回 nil 表示密码匹配，非 nil 表示不匹配或发生错误
	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(password)); err != nil {
		return LoginResult{}, ErrInvalidCredentials
	}
	token, expiresAt, err := s.tokenManager.Generate(user)
	if err != nil {
		return LoginResult{}, err
	}
	return LoginResult{
		AccessToken: token,
		ExpiresAt:   expiresAt,
		User:        sanitizeUser(user),
	}, nil
}

// CreateUser 创建一个新用户，返回创建的用户信息（不包含密码哈希）
func (s *Service) CreateUser(username string, password string, role Role) (User, error) {
	if s == nil || s.store == nil {
		return User{}, errors.New("认证服务未配置")
	}
	username = strings.TrimSpace(username)
	if username == "" || password == "" {
		return User{}, errors.New("用户名和密码不能为空")
	}
	if role == "" {
		role = RoleUser
	}
	if role != RoleAdmin && role != RoleUser {
		return User{}, errors.New("角色必须是 admin 或 user")
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return User{}, err
	}
	user, err := s.store.Create(username, string(hash), role)
	if err != nil {
		return User{}, err
	}
	return sanitizeUser(user), nil
}

// sanitizeUser 从用户信息中移除敏感字段（如密码哈希），以便安全地返回给前端
func sanitizeUser(user User) User {
	user.PasswordHash = ""
	return user
}
