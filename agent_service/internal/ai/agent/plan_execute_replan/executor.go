package plan_execute_replan

import (
	"context"
	"monitor-system/agent_service/internal/ai/models"
	"monitor-system/agent_service/internal/ai/tools"

	"github.com/cloudwego/eino/adk"
	"github.com/cloudwego/eino/adk/prebuilt/planexecute"
	"github.com/cloudwego/eino/components/tool"
	"github.com/cloudwego/eino/compose"
)

func NewExecutor(ctx context.Context) (adk.Agent, error) {
	toolList := []tool.BaseTool{
		tools.NewMonitorClusterOverviewTool(),
		tools.NewMonitorAnomaliesTool(),
		tools.NewMonitorPerformanceTool(),
		tools.NewMonitorTrendTool(),
		tools.NewMonitorDetailTool(),
		tools.NewGetCurrentTimeTool(),
	}
	toolList = append(toolList, tools.NewQueryInternalDocsTool())
	execModel, err := models.OpenAIForDeepSeekV3Quick(ctx)
	if err != nil {
		return nil, err
	}
	return planexecute.NewExecutor(ctx, &planexecute.ExecutorConfig{
		Model: execModel,
		ToolsConfig: adk.ToolsConfig{
			ToolsNodeConfig: compose.ToolsNodeConfig{
				Tools: toolList,
			},
		},
		MaxIterations: 999999,
	})
}
