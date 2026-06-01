package main

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/ai/pipeline/chat"
	"monitor-system/agent_service/internal/session/memory"
)

func main() {
	ctx := context.Background()
	scope := memory.MemoryScope{TenantID: "local", TeamID: "local", UserID: "tool", SessionID: "111"}
	manager := memory.NewInMemoryManager(memory.DefaultConfig())

	memCtx, err := manager.LoadContext(ctx, scope, "你好")
	if err != nil {
		panic(err)
	}
	userMessage := &chat.UserMessage{
		ID:      scope.SessionID,
		Query:   "你好",
		History: memCtx.RecentMessages,
		Summary: memCtx.Summary,
	}
	runner, err := chat.BuildChatAgent(ctx)
	if err != nil {
		panic(err)
	}
	out, err := runner.Invoke(ctx, userMessage)
	if err != nil {
		panic(err)
	}
	fmt.Println("Q: 你好")
	fmt.Println("A:", out.Content)
	if err := manager.AppendTurn(ctx, scope, "你好", out.Content); err != nil {
		panic(err)
	}

	memCtx, err = manager.LoadContext(ctx, scope, "现在是几点")
	if err != nil {
		panic(err)
	}
	userMessage = &chat.UserMessage{
		ID:      scope.SessionID,
		Query:   "现在是几点",
		History: memCtx.RecentMessages,
		Summary: memCtx.Summary,
	}
	out, err = runner.Invoke(ctx, userMessage)
	if err != nil {
		panic(err)
	}
	fmt.Println("----------------")
	fmt.Println("Q: 现在是几点")
	fmt.Println("A:", out.Content)
}
