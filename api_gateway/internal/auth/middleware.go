package auth

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

const (
	contextClaimsKey = "auth_claims"
	contextTokenKey  = "auth_token"
)

// JWT鉴权
func Middleware(tokenManager *TokenManager) gin.HandlerFunc {
	return func(c *gin.Context) {
		header := strings.TrimSpace(c.GetHeader("Authorization"))
		if header == "" {
			abortAuth(c, http.StatusUnauthorized, "未登录或登录已过期")
			return
		}
		token := strings.TrimSpace(strings.TrimPrefix(header, "Bearer "))
		if token == header || token == "" {
			abortAuth(c, http.StatusUnauthorized, "Authorization 必须使用 Bearer token")
			return
		}
		claims, err := tokenManager.Parse(token)
		if err != nil {
			abortAuth(c, http.StatusUnauthorized, "无效或已过期的 token")
			return
		}
		// 将 Claims 和 token 存入上下文，供后续处理使用
		c.Set(contextClaimsKey, claims)
		c.Set(contextTokenKey, token)
		c.Next()
	}
}

// RequireRole 需要特定角色的鉴权中间件
func RequireRole(role Role) gin.HandlerFunc {
	return func(c *gin.Context) {
		claims, ok := CurrentClaims(c)
		if !ok || claims.Role != role {
			abortAuth(c, http.StatusForbidden, "权限不足")
			return
		}
		c.Next()
	}
}

// CurrentClaims 从上下文中获取当前用户的 Claims
func CurrentClaims(c *gin.Context) (Claims, bool) {
	value, ok := c.Get(contextClaimsKey)
	if !ok {
		return Claims{}, false
	}
	claims, ok := value.(Claims)
	return claims, ok
}

func abortAuth(c *gin.Context, status int, message string) {
	c.AbortWithStatusJSON(status, gin.H{
		"code":    status,
		"message": message,
	})
}
