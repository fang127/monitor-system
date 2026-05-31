package chat

import (
	"context"
	"testing"

	"github.com/cloudwego/eino/schema"
)

func TestInputToChatIncludesMemoryContext(t *testing.T) {
	output, err := newInputToChatLambda(context.Background(), &UserMessage{
		ID:               "s1",
		Query:            "继续排查",
		History:          []*schema.Message{schema.UserMessage("上一轮问题")},
		Summary:          "用户正在排查 Redis 慢日志",
		LongTermMemories: "- [集群:prod / cluster_knowledge] 默认先查慢日志",
	})
	if err != nil {
		t.Fatalf("转换聊天输入失败: %v", err)
	}
	if output["summary"] != "用户正在排查 Redis 慢日志" {
		t.Fatalf("摘要未注入输出: %+v", output)
	}
	if output["long_term_memories"] == "" {
		t.Fatalf("长期记忆未注入输出: %+v", output)
	}
	if len(output["history"].([]*schema.Message)) != 1 {
		t.Fatalf("历史消息数量不正确: %+v", output)
	}
}
