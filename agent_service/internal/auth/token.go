package auth

import (
	"errors"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

// Claims 定义了 JWT 中包含的用户信息
type Claims struct {
	UserID   int64  `json:"user_id"`
	Username string `json:"username"`
	Role     string `json:"role"`
	TenantID string `json:"tenant_id,omitempty"`
	TeamID   string `json:"team_id,omitempty"`
}

// tokenClaims 是 JWT 中实际存储的结构，包含了用户信息和注册的标准声明
type tokenClaims struct {
	UserID   int64  `json:"user_id"`
	Username string `json:"username"`
	Role     string `json:"role"`
	TenantID string `json:"tenant_id,omitempty"`
	TeamID   string `json:"team_id,omitempty"`
	jwt.RegisteredClaims
}

// TokenManager 负责生成和解析 JWT token
type TokenManager struct {
	secret []byte
}

func NewTokenManager(secret string) *TokenManager {
	return &TokenManager{secret: []byte(secret)}
}

func (m *TokenManager) Generate(claims Claims, ttl time.Duration) (string, error) {
	if len(m.secret) == 0 {
		return "", errors.New("JWT_SECRET 不能为空")
	}
	now := time.Now()
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, tokenClaims{
		UserID:   claims.UserID,
		Username: claims.Username,
		Role:     claims.Role,
		TenantID: claims.TenantID,
		TeamID:   claims.TeamID,
		RegisteredClaims: jwt.RegisteredClaims{
			Subject:   claims.Username,
			IssuedAt:  jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(now.Add(ttl)),
		},
	})
	return token.SignedString(m.secret)
}

func (m *TokenManager) Parse(rawToken string) (Claims, error) {
	if len(m.secret) == 0 {
		return Claims{}, errors.New("JWT_SECRET 不能为空")
	}
	claims := &tokenClaims{}
	// keyFunc 用于验证 token 的签名和算法
	// 我们只允许使用 HS256 签名算法，并且使用预设的 secret 进行验证
	// 如果 token 的签名算法不符合预期，或者 token 无效或过期，我们都会返回错误
	token, err := jwt.ParseWithClaims(rawToken, claims, func(token *jwt.Token) (interface{}, error) {
		if token.Method != jwt.SigningMethodHS256 {
			return nil, errors.New("无效签名算法")
		}
		return m.secret, nil
	})
	if err != nil || token == nil || !token.Valid {
		return Claims{}, errors.New("无效或已过期的 token")
	}
	return Claims{
		UserID:   claims.UserID,
		Username: claims.Username,
		Role:     claims.Role,
		TenantID: claims.TenantID,
		TeamID:   claims.TeamID,
	}, nil
}
