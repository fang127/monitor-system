package agent

import (
	"context"
	"monitor-system/agent_service/internal/session/memory"
	"monitor-system/agent_service/internal/sse"

	"github.com/gin-gonic/gin"
)

type ControllerV1 struct {
	service       *sse.Service
	memoryManager memory.MemoryManager
	chatBuilder   chatAgentBuilder
}

// 创建新的ControllerV1实例
func NewV1() *ControllerV1 {
	cfg, err := memory.LoadConfig(context.Background())
	if err != nil {
		cfg = memory.DefaultConfig()
	}
	return NewV1WithMemoryManager(memory.NewInMemoryManager(cfg))
}

// NewV1WithMemoryManager 创建带有指定记忆管理器的 ControllerV1，主要用于测试和后续替换持久化实现。
func NewV1WithMemoryManager(memoryManager memory.MemoryManager) *ControllerV1 {
	if memoryManager == nil {
		memoryManager = memory.NewInMemoryManager(memory.DefaultConfig())
	}
	return &ControllerV1{
		service:       sse.New(),
		memoryManager: memoryManager,
		chatBuilder:   defaultChatAgentBuilder,
	}
}

// RegisterRoutes 注册路由
func (c *ControllerV1) RegisterRoutes(group *gin.RouterGroup) {
	group.POST("/chat", c.Chat)                    // 处理普通聊天请求
	group.POST("/chat_stream", c.ChatStream)       // 处理聊天流请求
	group.POST("/upload", c.FileUpload)            // 处理文件上传请求
	group.POST("/ai_ops", c.AIOps)                 // 处理AI Ops请求
	group.GET("/memory", c.ListMemories)           // 查询长期记忆
	group.PUT("/memory/policy", c.SetMemoryPolicy) // 设置记忆策略
	group.DELETE("/memory/:id", c.DeleteMemory)    // 删除指定记忆
	group.DELETE("/memory", c.ClearMemories)       // 清除所有记忆
}
