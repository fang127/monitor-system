package chat

import (
	"github.com/gin-gonic/gin"
)

// 该文件定义了聊天相关的接口

// IChatV1 定义了聊天相关的接口，包括普通聊天、流式聊天、文件上传和AIOps功能。
type IChatV1 interface {
	Chat(ctx *gin.Context)
	ChatStream(ctx *gin.Context)
	FileUpload(ctx *gin.Context)
	AIOps(ctx *gin.Context)
}
