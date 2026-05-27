package httpapi

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"path/filepath"
	"strings"

	"github.com/gin-gonic/gin"

	"monitor-system/agent_service/internal/agent"
	"monitor-system/agent_service/internal/knowledge"
	"monitor-system/agent_service/internal/model"
)

type Agent interface {
	Chat(ctx context.Context, req agent.ChatRequest) (agent.ChatResponse, error)
	ChatStream(ctx context.Context, req agent.ChatRequest) (<-chan model.StreamChunk, error)
	Upload(ctx context.Context, path string) (knowledge.IndexResult, error)
	AIOps(ctx context.Context) (agent.AIOpsResponse, error)
}

type Options struct {
	DocsDir string
}

type envelope struct {
	Code    int             `json:"code"`
	Message string          `json:"message"`
	Data    json.RawMessage `json:"data,omitempty"`
}

type chatPayload struct {
	ID       string `json:"Id"`
	Question string `json:"Question"`
}

func NewRouter(agentSvc Agent) *gin.Engine {
	return NewRouterWithOptions(agentSvc, Options{DocsDir: "./docs"})
}

func NewRouterWithOptions(agentSvc Agent, opts Options) *gin.Engine {
	gin.SetMode(gin.ReleaseMode)
	router := gin.New()
	router.Use(gin.Logger(), gin.Recovery(), cors())

	router.GET("/health", func(c *gin.Context) {
		ok(c, gin.H{"status": "ok", "service": "agent_service"})
	})

	group := router.Group("/api/agent")
	group.POST("/chat", func(c *gin.Context) {
		var req chatPayload
		if err := c.ShouldBindJSON(&req); err != nil {
			fail(c, http.StatusBadRequest, fmt.Errorf("解析请求失败: %w", err))
			return
		}
		resp, err := agentSvc.Chat(c.Request.Context(), agent.ChatRequest{ID: req.ID, Question: req.Question})
		if err != nil {
			fail(c, http.StatusBadRequest, err)
			return
		}
		ok(c, resp)
	})
	group.POST("/chat_stream", func(c *gin.Context) {
		var req chatPayload
		if err := c.ShouldBindJSON(&req); err != nil {
			fail(c, http.StatusBadRequest, fmt.Errorf("解析请求失败: %w", err))
			return
		}
		stream, err := agentSvc.ChatStream(c.Request.Context(), agent.ChatRequest{ID: req.ID, Question: req.Question})
		if err != nil {
			fail(c, http.StatusBadRequest, err)
			return
		}
		writeSSE(c, stream)
	})
	group.POST("/upload", func(c *gin.Context) {
		file, err := c.FormFile("file")
		if err != nil {
			fail(c, http.StatusBadRequest, fmt.Errorf("请上传文件"))
			return
		}
		path, err := knowledge.SafeUploadPath(opts.DocsDir, filepath.Base(file.Filename))
		if err != nil {
			fail(c, http.StatusBadRequest, err)
			return
		}
		if err := c.SaveUploadedFile(file, path); err != nil {
			fail(c, http.StatusInternalServerError, err)
			return
		}
		result, err := agentSvc.Upload(c.Request.Context(), path)
		if err != nil {
			fail(c, http.StatusInternalServerError, err)
			return
		}
		payload := gin.H{"fileName": filepath.Base(path), "filePath": path, "fileSize": file.Size, "chunks": result.Chunks}
		if result.Warning != "" {
			payload["warning"] = result.Warning
		}
		ok(c, payload)
	})
	group.POST("/ai_ops", func(c *gin.Context) {
		resp, err := agentSvc.AIOps(c.Request.Context())
		if err != nil {
			fail(c, http.StatusInternalServerError, err)
			return
		}
		ok(c, resp)
	})

	return router
}

func cors() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Writer.Header().Set("Access-Control-Allow-Origin", "*")
		c.Writer.Header().Set("Access-Control-Allow-Methods", "GET,POST,PUT,PATCH,DELETE,OPTIONS")
		c.Writer.Header().Set("Access-Control-Allow-Headers", "Origin,Content-Type,Accept,Authorization")
		if c.Request.Method == http.MethodOptions {
			c.AbortWithStatus(http.StatusNoContent)
			return
		}
		c.Next()
	}
}

func ok(c *gin.Context, data any) {
	raw, _ := json.Marshal(data)
	c.JSON(http.StatusOK, envelope{Code: 0, Message: "OK", Data: raw})
}

func fail(c *gin.Context, status int, err error) {
	c.JSON(status, gin.H{"code": status, "message": err.Error()})
}

func writeSSE(c *gin.Context, stream <-chan model.StreamChunk) {
	writer := c.Writer
	header := writer.Header()
	header.Set("Content-Type", "text/event-stream")
	header.Set("Cache-Control", "no-cache")
	header.Set("Connection", "keep-alive")
	header.Set("Access-Control-Allow-Origin", "*")
	flusher, _ := writer.(http.Flusher)
	for chunk := range stream {
		if chunk.Err != nil {
			writeEvent(writer, "error", chunk.Err.Error())
			if flusher != nil {
				flusher.Flush()
			}
			return
		}
		writeEvent(writer, "message", chunk.Content)
		if flusher != nil {
			flusher.Flush()
		}
	}
	writeEvent(writer, "done", "Stream completed")
	if flusher != nil {
		flusher.Flush()
	}
}

func writeEvent(w http.ResponseWriter, event string, data string) {
	_, _ = fmt.Fprintf(w, "event: %s\n", event)
	for _, line := range strings.Split(data, "\n") {
		_, _ = fmt.Fprintf(w, "data: %s\n", line)
	}
	_, _ = fmt.Fprint(w, "\n")
}
