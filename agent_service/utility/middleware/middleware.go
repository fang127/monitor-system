package middleware

import "github.com/gogf/gf/v2/net/ghttp"

// CORSMiddleware 处理CORS跨域请求
func CORSMiddleware(r *ghttp.Request) {
	r.Response.CORSDefault()
	r.Middleware.Next()
}

func ResponseMiddleware(r *ghttp.Request) {
	r.Middleware.Next()

	var (
		msg string
		res = r.GetHandlerResponse()
		err = r.GetError()
	)
	if err != nil {
		msg = err.Error()
	} else {
		msg = "OK"
	}
	code := 0
	if err != nil {
		code = 1
	}
	r.Response.WriteJson(Response{
		Code:    code,
		Message: msg,
		Data:    res,
	})
}

type Response struct {
	Code    int         `json:"code"    dc:"状态码，0表示成功"`
	Message string      `json:"message" dc:"消息提示"`
	Data    interface{} `json:"data"    dc:"执行结果"`
}
