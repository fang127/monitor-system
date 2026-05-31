package agent

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/ai/callback"
	"monitor-system/agent_service/internal/ai/pipeline/chat"
	"monitor-system/agent_service/internal/handler/agent/dto"
	"monitor-system/agent_service/internal/response"
	"strings"

	"github.com/cloudwego/eino/compose"
	"github.com/gin-gonic/gin"
)

// 普通对话接口实现

// Chat 接口处理用户的对话请求，解析请求参数，调用 runChat 方法执行对话逻辑，并返回结果给客户端
func (c *ControllerV1) Chat(gctx *gin.Context) {
	var req dto.ChatReq
	if err := gctx.ShouldBindJSON(&req); err != nil {
		response.Respond(gctx, nil, fmt.Errorf("解析请求失败: %w", err))
		return
	}
	res, err := c.runChat(gctx.Request.Context(), &req)
	response.Respond(gctx, res, err)
}

func (c *ControllerV1) runChat(ctx context.Context, req *dto.ChatReq) (res *dto.ChatRes, err error) {
	scope := memoryScopeFromChatReq(req)
	msg := strings.TrimSpace(req.Question)
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
		return nil, err
	}

	out, err := runner.Invoke(ctx, userMessage, compose.WithCallbacks(callback.LogCallback(nil)))
	if err != nil {
		if isModelRateLimitError(err) {
			answer := "当前 AI 模型服务触发 RPM 限流，暂时无法继续生成回复。请稍后重试，或更换可用的 API Key / 模型服务端点。"
			return &dto.ChatRes{Answer: answer}, nil
		}
		return nil, err
	}
	res = &dto.ChatRes{
		Answer: out.Content,
	}
	_ = c.memoryManager.AppendTurn(ctx, scope, msg, out.Content)
	_ = c.memoryManager.ExtractDurableMemories(ctx, scope)

	return res, nil
}

func isModelRateLimitError(err error) bool {
	message := err.Error()
	return strings.Contains(message, "429") ||
		strings.Contains(message, "Too Many Requests") ||
		strings.Contains(message, "RPM limit")
}
