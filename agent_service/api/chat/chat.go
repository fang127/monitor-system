package chat

import (
	"github.com/gin-gonic/gin"
)

type IChatV1 interface {
	Chat(ctx *gin.Context)
	ChatStream(ctx *gin.Context)
	FileUpload(ctx *gin.Context)
	AIOps(ctx *gin.Context)
}
