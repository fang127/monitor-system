package chat

import (
	"context"
	"time"
)

// 定义 Graph 中的数据转换 Lambda

func newInputToRagLambda(ctx context.Context, input *UserMessage, opts ...any) (output string, err error) {
	// TODO
	// 这里可以根据需要对输入进行处理，例如提取关键词、生成查询等
	// 也可以Rewrite Query
	return input.Query, nil
}

func newInputToChatLambda(ctx context.Context, input *UserMessage, opts ...any) (output map[string]any, err error) {
	return map[string]any{
		"content":            input.Query,
		"history":            input.History,
		"summary":            input.Summary,
		"long_term_memories": input.LongTermMemories,
		"date":               time.Now().Format("2006-01-02 15:04:05"),
	}, nil
}
