package chat

import (
	"context"
	"errors"
	"fmt"
	"io"
	v1 "monitor-system/agent_service/api/chat/v1"
	"monitor-system/agent_service/internal/ai/agent/chat_pipeline"
	"monitor-system/agent_service/utility/log_call_back"
	"monitor-system/agent_service/utility/mem"
	"monitor-system/agent_service/utility/middleware"
	"strings"

	"github.com/cloudwego/eino/compose"
	"github.com/cloudwego/eino/schema"
	"github.com/gin-gonic/gin"
)

// 流式对话接口实现。

// Chat 处理用户的聊天请求，调用 AI 模型生成回复，并返回给用户。
func (c *ControllerV1) ChatStream(gctx *gin.Context) {
	var req v1.ChatStreamReq
	if err := gctx.ShouldBindJSON(&req); err != nil {
		middleware.Respond(gctx, nil, fmt.Errorf("解析请求失败: %w", err))
		return
	}
	if err := c.runChatStream(gctx.Request.Context(), gctx, &req); err != nil {
		return
	}
}

func (c *ControllerV1) runChatStream(ctx context.Context, gctx *gin.Context, req *v1.ChatStreamReq) error {
	id := req.Id
	msg := req.Question

	ctx = context.WithValue(ctx, "client_id", req.Id)
	client, err := c.service.Create(ctx, gctx.Writer, req.Id)
	if err != nil {
		middleware.Respond(gctx, nil, err)
		return err
	}

	userMessage := &chat_pipeline.UserMessage{
		ID:      id,
		Query:   msg,
		History: mem.GetSimpleMemory(id).GetMessages(),
	}

	runner, err := chat_pipeline.BuildChatAgent(ctx)
	if err != nil {
		client.SendToClient("error", err.Error())
		return err
	}
	sr, err := runner.Stream(ctx, userMessage, compose.WithCallbacks(log_call_back.LogCallback(nil)))
	if err != nil {
		client.SendToClient("error", err.Error())
		return err
	}
	defer sr.Close()

	var fullResponse strings.Builder

	defer func() {
		completeResponse := fullResponse.String()
		if completeResponse != "" {
			mem.GetSimpleMemory(id).SetMessages(schema.UserMessage(msg))
			mem.GetSimpleMemory(id).SetMessages(schema.SystemMessage(completeResponse))
		}
	}()

	for {
		chunk, err := sr.Recv()
		if errors.Is(err, io.EOF) {
			client.SendToClient("done", "Stream completed")
			return nil
		}
		if err != nil {
			client.SendToClient("error", err.Error())
			return err
		}
		fullResponse.WriteString(chunk.Content)
		client.SendToClient("message", chunk.Content)
	}
}
