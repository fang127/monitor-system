package main

import (
	"monitor-system/agent_service/internal/controller/chat"
	"monitor-system/agent_service/utility/common"
	"monitor-system/agent_service/utility/middleware"

	"github.com/gogf/gf/v2/frame/g"
	"github.com/gogf/gf/v2/net/ghttp"
	"github.com/gogf/gf/v2/os/gctx"
)

func main() {
	ctx := gctx.New()
	fileDir, err := common.ConfigString(ctx, "docs_dir", "AGENT_DOCS_DIR", "./docs")
	if err != nil {
		panic(err)
	}
	common.FileDir = fileDir
	port, err := common.ConfigInt(ctx, "agent_service_port", "AGENT_SERVICE_PORT", 6872)
	if err != nil {
		panic(err)
	}
	s := g.Server()
	s.Group("/api/agent", func(group *ghttp.RouterGroup) {
		group.Middleware(middleware.CORSMiddleware)
		group.Middleware(middleware.ResponseMiddleware)
		group.Bind(chat.NewV1())
	})
	s.SetPort(port)
	s.Run()
}
