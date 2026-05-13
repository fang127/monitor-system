package chat

import (
	"monitor-system/agent_service/internal/logic/sse"

	"github.com/gin-gonic/gin"
)

type ControllerV1 struct {
	service *sse.Service
}

func NewV1() *ControllerV1 {
	return &ControllerV1{
		service: sse.New(),
	}
}

func (c *ControllerV1) RegisterRoutes(group *gin.RouterGroup) {
	group.POST("/chat", c.Chat)
	group.POST("/chat_stream", c.ChatStream)
	group.POST("/upload", c.FileUpload)
	group.POST("/ai_ops", c.AIOps)
}
