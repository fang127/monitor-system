package auth

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

func Middleware(tokenManager *TokenManager) gin.HandlerFunc {
	return func(c *gin.Context) {
		header := strings.TrimSpace(c.GetHeader("Authorization"))
		if header == "" {
			abort(c, "未登录或登录已过期")
			return
		}
		token := strings.TrimSpace(strings.TrimPrefix(header, "Bearer "))
		if token == "" || token == header {
			abort(c, "Authorization 必须使用 Bearer token")
			return
		}
		if _, err := tokenManager.Parse(token); err != nil {
			abort(c, "无效或已过期的 token")
			return
		}
		c.Request = c.Request.WithContext(ContextWithBearerToken(c.Request.Context(), token))
		c.Next()
	}
}

func abort(c *gin.Context, message string) {
	c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{
		"code":    http.StatusUnauthorized,
		"message": message,
	})
}
