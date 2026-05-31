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
		claims, err := tokenManager.Parse(token)
		if err != nil {
			abort(c, "无效或已过期的 token")
			return
		}
		ctx := ContextWithBearerToken(c.Request.Context(), token) // 将 token 存入上下文，方便后续使用
		ctx = ContextWithClaims(ctx, claims)                      // 将解析出的 claims 存入上下文，方便后续使用
		c.Request = c.Request.WithContext(ctx)                    // 更新请求的上下文
		c.Next()                                                  // 继续处理请求
	}
}

func abort(c *gin.Context, message string) {
	c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{
		"code":    http.StatusUnauthorized,
		"message": message,
	})
}
