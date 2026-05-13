package middleware

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// CORSMiddleware 处理CORS跨域请求。
func CORSMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Writer.Header().Set("Access-Control-Allow-Origin", "*")                                         // 允许所有来源
		c.Writer.Header().Set("Access-Control-Allow-Methods", "GET,POST,PUT,PATCH,DELETE,OPTIONS")        // 允许的HTTP方法
		c.Writer.Header().Set("Access-Control-Allow-Headers", "Origin,Content-Type,Accept,Authorization") // 允许的请求头
		c.Writer.Header().Set("Access-Control-Allow-Credentials", "false")                                // 是否允许携带凭证
		// 处理预检请求，直接返回204 No Content
		if c.Request.Method == http.MethodOptions {
			c.AbortWithStatus(http.StatusNoContent)
			return
		}
		c.Next()
	}
}

// Respond 统一API响应格式。
func Respond(c *gin.Context, res interface{}, err error) {
	msg := "OK"
	code := 0
	if err != nil {
		msg = err.Error()
		code = 1
	}
	c.JSON(http.StatusOK, Response{
		Code:    code,
		Message: msg,
		Data:    res,
	})
}

type Response struct {
	Code    int         `json:"code"`
	Message string      `json:"message"`
	Data    interface{} `json:"data"`
}
