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

// 创建一个新的执行器实例，配置所需的工具和模型
func NewExecutor(ctx context.Context) (adk.Agent, error) {
	// 配置工具列表，包含监控相关的工具和查询内部文档的工具
	toolList := []tool.BaseTool{
		tools.NewMonitorClusterOverviewTool(), // 监控集群概览工具
		tools.NewMonitorAnomaliesTool(),       // 监控异常工具
		tools.NewMonitorPerformanceTool(),     // 监控性能工具
		tools.NewMonitorTrendTool(),           // 监控趋势工具
		tools.NewMonitorDetailTool(),          // 监控详情工具
		tools.NewMonitorMysqlDetailTool(),     // MySQL 监控详情工具
		tools.NewGetCurrentTimeTool(),         // 获取当前时间工具
	}
	// 添加查询内部文档的工具，以便在执行过程中能够访问相关文档信息
	toolList = append(toolList, tools.NewQueryInternalDocsTool())
	// 创建执行器实例
	execModel, err := models.OpenAIForQuick(ctx)
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
		MaxIterations: 999999, // 设置最大迭代次数，确保执行器能够持续执行直到完成任务
	})
}
