package auth

import (
	"errors"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

// JWT中提取的用户信息
type tokenClaims struct {
	UserID   int64  `json:"user_id"`
	Username string `json:"username"`
	Role     Role   `json:"role"`
	TenantID string `json:"tenant_id"`
	TeamID   string `json:"team_id"`
	jwt.RegisteredClaims
}

// TokenManager负责生成和解析JWT
type TokenManager struct {
	secret []byte
	ttl    time.Duration
}

// NewTokenManager创建一个新的TokenManager实例
func NewTokenManager(secret string, ttl time.Duration) *TokenManager {
	return &TokenManager{
		secret: []byte(secret),
		ttl:    ttl,
	}
}

// Generate生成一个新的JWT，返回token字符串和过期时间
func (m *TokenManager) Generate(user User, scope TeamMembership) (string, time.Time, error) {
	if len(m.secret) == 0 {
		return "", time.Time{}, errors.New("JWT_SECRET 不能为空")
	}
	if scope.TenantID == "" || scope.TeamID == "" {
		return "", time.Time{}, ErrTeamRequired
	}
	now := time.Now()
	expiresAt := now.Add(m.ttl)
	claims := tokenClaims{
		UserID:   user.ID,
		Username: user.Username,
		Role:     user.Role,
		TenantID: scope.TenantID,
		TeamID:   scope.TeamID,
		RegisteredClaims: jwt.RegisteredClaims{
			Subject:   user.Username,
			IssuedAt:  jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(expiresAt),
		},
	}
	// NewWithClaims创建一个新的JWT，指定签名算法和claims，然后调用SignedString方法使用secret签名生成最终的token字符串
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	signed, err := token.SignedString(m.secret)
	if err != nil {
		return "", time.Time{}, err
	}
	return signed, expiresAt, nil
}

// Parse解析JWT，返回Claims结构体和错误信息
func (m *TokenManager) Parse(rawToken string) (Claims, error) {
	if len(m.secret) == 0 {
		return Claims{}, ErrInvalidToken
	}
	claims := &tokenClaims{}
	// ParseWithClaims会验证token的签名和过期时间，如果无效会返回错误
	// keyFunc用于提供验证token签名所需的密钥，这里我们检查签名算法是否正确，并返回secret
	token, err := jwt.ParseWithClaims(rawToken, claims, func(token *jwt.Token) (interface{}, error) {
		if token.Method != jwt.SigningMethodHS256 {
			return nil, ErrInvalidToken
		}
		return m.secret, nil
	})
	if err != nil || token == nil || !token.Valid {
		return Claims{}, ErrInvalidToken
	}
	if claims.UserID == 0 || claims.Username == "" || claims.Role == "" || claims.TenantID == "" || claims.TeamID == "" {
		return Claims{}, ErrInvalidToken
	}
	return Claims{
		UserID:   claims.UserID,
		Username: claims.Username,
		Role:     claims.Role,
		TenantID: claims.TenantID,
		TeamID:   claims.TeamID,
	}, nil
}
