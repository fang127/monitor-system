package chat_pipeline

import "github.com/cloudwego/eino/schema"

// 用户输入的消息结构体
type UserMessage struct {
	ID      string            `json:"id"`
	Query   string            `json:"query"`
	History []*schema.Message `json:"history"`
}
