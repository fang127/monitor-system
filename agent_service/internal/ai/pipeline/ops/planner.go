package ops

import (
	"context"
	"monitor-system/agent_service/internal/ai/models"

	"github.com/cloudwego/eino/adk"
	"github.com/cloudwego/eino/adk/prebuilt/planexecute"
)

// NewPlanner 创建一个新的 Planner 实例，使用 OpenAI 模型进行工具调用。
func NewPlanner(ctx context.Context) (adk.Agent, error) {
	planModel, err := models.OpenAIForThink(ctx)
	if err != nil {
		return nil, err
	}
	return planexecute.NewPlanner(ctx, &planexecute.PlannerConfig{
		ToolCallingChatModel: planModel,
	})
}
