package auth

import (
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
)

func TestMiddlewareRejectsMissingToken(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router := gin.New()
	router.Use(Middleware(NewTokenManager("test-secret")))
	router.GET("/api/agent/ping", func(c *gin.Context) {
		c.Status(http.StatusOK)
	})

	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, "/api/agent/ping", nil))
	if recorder.Code != http.StatusUnauthorized {
		t.Fatalf("未携带 token 状态码 = %d, want %d", recorder.Code, http.StatusUnauthorized)
	}
}

func TestMiddlewareAcceptsTokenAndStoresItInRequestContext(t *testing.T) {
	gin.SetMode(gin.TestMode)
	tokenManager := NewTokenManager("test-secret")
	token, err := tokenManager.Generate(Claims{
		UserID:   12,
		Username: "alice",
		Role:     "user",
	}, time.Hour)
	if err != nil {
		t.Fatalf("生成测试 token 失败: %v", err)
	}

	router := gin.New()
	router.Use(Middleware(tokenManager))
	router.GET("/api/agent/ping", func(c *gin.Context) {
		if got, ok := BearerTokenFromContext(c.Request.Context()); !ok || got != token {
			t.Fatalf("context 中的 token = %q, ok=%v", got, ok)
		}
		c.Status(http.StatusOK)
	})

	req := httptest.NewRequest(http.MethodGet, "/api/agent/ping", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, req)
	if recorder.Code != http.StatusOK {
		t.Fatalf("有效 token 状态码 = %d, want %d", recorder.Code, http.StatusOK)
	}
}
