package plan_execute_replan

import (
	"context"
	"monitor-system/agent_service/internal/ai/models"

	"github.com/cloudwego/eino/adk"
	"github.com/cloudwego/eino/adk/prebuilt/planexecute"
)

// NewRePlanAgent 创建一个新的重新规划智能体实例，该实例使用 OpenAI 模型进行思考和重新规划。它接受一个上下文参数，并返回一个实现了 adk.Agent 接口的实例或一个错误。
func NewRePlanAgent(ctx context.Context) (adk.Agent, error) {
	model, err := models.OpenAIForThink(ctx)
	if err != nil {
		return nil, err
	}
	return planexecute.NewReplanner(ctx, &planexecute.ReplannerConfig{
		ChatModel: model,
	})
}
