package response

import "github.com/gin-gonic/gin"

type Body struct {
	Code    int         `json:"code"`
	Message string      `json:"message"`
	Data    interface{} `json:"data,omitempty"`
}

func OK(c *gin.Context, status int, data interface{}) {
	c.JSON(status, Body{
		Code:    0,
		Message: "ok",
		Data:    data,
	})
}

func Error(c *gin.Context, status int, message string) {
	c.JSON(status, Body{
		Code:    status,
		Message: message,
	})
}
