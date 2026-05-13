package chat_pipeline

import (
	"context"
	"monitor-system/agent_service/internal/ai/models"

	"github.com/cloudwego/eino/components/model"
)

func newChatModel(ctx context.Context) (cm model.ToolCallingChatModel, err error) {
	cm, err = models.OpenAIForDeepSeekV3Quick(ctx)
	if err != nil {
		return nil, err
	}
	return cm, nil
}
