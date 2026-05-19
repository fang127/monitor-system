package response

import "github.com/gin-gonic/gin"

// 响应体
type Body struct {
	Code    int         `json:"code"`           // 0 表示成功，非 0 表示错误
	Message string      `json:"message"`        // 错误信息，成功时为 "ok"
	Data    interface{} `json:"data,omitempty"` // 成功时返回的数据，错误时不返回
}

// OK 响应成功
// 结果类似：
//
//	{
//	  "code": 0,
//	  "message": "ok",
//	  "data": {
//	    "id": 1,
//	    "name": "示例用户"
//	  }
//	}
func OK(c *gin.Context, status int, data interface{}) {
	c.JSON(status, Body{
		Code:    0,
		Message: "ok",
		Data:    data,
	})
}

// Error 响应错误
// 结果类似：
//
//	{
//	  "code": 400,
//	  "message": "错误请求"
//	}
func Error(c *gin.Context, status int, message string) {
	c.JSON(status, Body{
		Code:    status,
		Message: message,
	})
}
