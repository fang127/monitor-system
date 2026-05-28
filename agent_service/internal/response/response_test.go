package response

import (
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/gin-gonic/gin"
)

func TestRespondKeepsCompatibleEnvelope(t *testing.T) {
	gin.SetMode(gin.TestMode)
	recorder := httptest.NewRecorder()
	ctx, _ := gin.CreateTestContext(recorder)

	Respond(ctx, gin.H{"answer": "好的"}, nil)

	if recorder.Code != http.StatusOK {
		t.Fatalf("响应状态码应保持 200，got=%d", recorder.Code)
	}
	var body Response
	if err := json.Unmarshal(recorder.Body.Bytes(), &body); err != nil {
		t.Fatalf("解析响应失败: %v", err)
	}
	if body.Code != 0 || body.Message != "OK" {
		t.Fatalf("成功响应 envelope 不兼容: %+v", body)
	}
}

func TestRespondErrorKeepsCompatibleEnvelope(t *testing.T) {
	gin.SetMode(gin.TestMode)
	recorder := httptest.NewRecorder()
	ctx, _ := gin.CreateTestContext(recorder)

	Respond(ctx, nil, errors.New("参数错误"))

	var body Response
	if err := json.Unmarshal(recorder.Body.Bytes(), &body); err != nil {
		t.Fatalf("解析响应失败: %v", err)
	}
	if body.Code != 1 || body.Message != "参数错误" {
		t.Fatalf("错误响应 envelope 不兼容: %+v", body)
	}
}

func TestCORSMiddlewareHandlesPreflight(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router := gin.New()
	router.Use(CORSMiddleware())
	router.OPTIONS("/api/agent/chat", func(c *gin.Context) {
		c.Status(http.StatusAccepted)
	})

	req := httptest.NewRequest(http.MethodOptions, "/api/agent/chat", nil)
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, req)

	if recorder.Code != http.StatusNoContent {
		t.Fatalf("预检请求应直接返回 204，got=%d", recorder.Code)
	}
	if recorder.Header().Get("Access-Control-Allow-Origin") != "*" {
		t.Fatal("缺少 CORS 响应头")
	}
}
