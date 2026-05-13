package chat

import (
	"monitor-system/agent_service/internal/logic/sse"

	"github.com/gin-gonic/gin"
)

type ControllerV1 struct {
	service *sse.Service
}

// 创建新的ControllerV1实例
func NewV1() *ControllerV1 {
	return &ControllerV1{
		service: sse.New(),
	}
}

// RegisterRoutes 注册路由
func (c *ControllerV1) RegisterRoutes(group *gin.RouterGroup) {
	group.POST("/chat", c.Chat)              // 处理普通聊天请求
	group.POST("/chat_stream", c.ChatStream) // 处理聊天流请求
	group.POST("/upload", c.FileUpload)      // 处理文件上传请求
	group.POST("/ai_ops", c.AIOps)           // 处理AI Ops请求
}
