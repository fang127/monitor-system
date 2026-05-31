package agent

import (
	"context"
	"monitor-system/agent_service/internal/ai/pipeline/chat"

	"github.com/cloudwego/eino/compose"
	"github.com/cloudwego/eino/schema"
)

type chatAgentBuilder func(context.Context) (compose.Runnable[*chat.UserMessage, *schema.Message], error)

func defaultChatAgentBuilder(ctx context.Context) (compose.Runnable[*chat.UserMessage, *schema.Message], error) {
	return chat.BuildChatAgent(ctx)
}
