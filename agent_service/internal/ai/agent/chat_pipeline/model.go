package chat_pipeline

import (
	"context"
	"monitor-system/agent_service/internal/ai/models"

	"github.com/cloudwego/eino/components/model"
)

// newChatModel 创建一个新的聊天模型实例
func newChatModel(ctx context.Context) (cm model.ToolCallingChatModel, err error) {
	cm, err = models.OpenAIForQuick(ctx)
	if err != nil {
		return nil, err
	}
	return cm, nil
}
