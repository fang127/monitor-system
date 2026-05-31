package agent

import (
	"context"
	"errors"
	"fmt"
	"io"
	"monitor-system/agent_service/internal/ai/callback"
	"monitor-system/agent_service/internal/ai/pipeline/chat"
	"monitor-system/agent_service/internal/handler/agent/dto"
	"monitor-system/agent_service/internal/response"
	"strings"

	"github.com/cloudwego/eino/compose"
	"github.com/gin-gonic/gin"
)

// 流式对话接口实现。

// Chat 处理用户的聊天请求，调用 AI 模型生成回复，并返回给用户。
func (c *ControllerV1) ChatStream(gctx *gin.Context) {
	var req dto.ChatStreamReq
	if err := gctx.ShouldBindJSON(&req); err != nil {
		response.Respond(gctx, nil, fmt.Errorf("解析请求失败: %w", err))
		return
	}
	if err := c.runChatStream(gctx.Request.Context(), gctx, &req); err != nil {
		return
	}
}

func (c *ControllerV1) runChatStream(ctx context.Context, gctx *gin.Context, req *dto.ChatStreamReq) error {
	scope := memoryScopeFromChatStreamReq(req)
	msg := req.Question

	ctx = context.WithValue(ctx, "client_id", req.Id)
	client, err := c.service.Create(ctx, gctx.Writer, req.Id)
	if err != nil {
		response.Respond(gctx, nil, err)
		return err
	}
	memCtx, err := c.memoryManager.LoadContext(ctx, scope, msg)
	if err != nil {
		memCtx = emptyMemoryContext(scope)
	}

	userMessage := &chat.UserMessage{
		ID:               scope.SessionID,
		Query:            msg,
		History:          memCtx.RecentMessages,
		Summary:          memCtx.Summary,
		LongTermMemories: formatLongTermForPrompt(memCtx.LongTerm),
	}

	runner, err := c.chatBuilder(ctx)
	if err != nil {
		client.SendToClient("error", err.Error())
		return err
	}
	sr, err := runner.Stream(ctx, userMessage, compose.WithCallbacks(callback.LogCallback(nil)))
	if err != nil {
		client.SendToClient("error", err.Error())
		return err
	}
	defer sr.Close()

	var fullResponse strings.Builder

	defer func() {
		completeResponse := fullResponse.String()
		if completeResponse != "" {
			_ = c.memoryManager.AppendTurn(ctx, scope, msg, completeResponse)
			_ = c.memoryManager.ExtractDurableMemories(ctx, scope)
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
