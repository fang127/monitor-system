package chat

import (
	"context"
	"fmt"
	"monitor-system/agent_service/api/chat/v1"
	"monitor-system/agent_service/internal/ai/agent/chat_pipeline"
	"monitor-system/agent_service/utility/log_call_back"
	"monitor-system/agent_service/utility/mem"
	"monitor-system/agent_service/utility/middleware"

	"github.com/cloudwego/eino/compose"
	"github.com/cloudwego/eino/schema"
	"github.com/gin-gonic/gin"
)

func (c *ControllerV1) Chat(gctx *gin.Context) {
	var req v1.ChatReq
	if err := gctx.ShouldBindJSON(&req); err != nil {
		middleware.Respond(gctx, nil, fmt.Errorf("解析请求失败: %w", err))
		return
	}
	res, err := c.runChat(gctx.Request.Context(), &req)
	middleware.Respond(gctx, res, err)
}

func (c *ControllerV1) runChat(ctx context.Context, req *v1.ChatReq) (res *v1.ChatRes, err error) {
	id := req.Id
	msg := req.Question
	userMessage := &chat_pipeline.UserMessage{
		ID:      id,
		Query:   msg,
		History: mem.GetSimpleMemory(id).GetMessages(),
	}

	runner, err := chat_pipeline.BuildChatAgent(ctx)
	if err != nil {
		return nil, err
	}

	out, err := runner.Invoke(ctx, userMessage, compose.WithCallbacks(log_call_back.LogCallback(nil)))
	if err != nil {
		return nil, err
	}
	res = &v1.ChatRes{
		Answer: out.Content,
	}
	mem.GetSimpleMemory(id).SetMessages(schema.UserMessage(msg))
	mem.GetSimpleMemory(id).SetMessages(schema.SystemMessage(out.Content))

	return res, nil
}
