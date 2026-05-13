package chat_pipeline

import (
	"context"
	"monitor-system/agent_service/internal/ai/tools"

	"github.com/cloudwego/eino/compose"
	"github.com/cloudwego/eino/flow/agent/react"
)

func newReactAgentLambda(ctx context.Context) (lba *compose.Lambda, err error) {
	config := &react.AgentConfig{
		MaxStep:            25,
		ToolReturnDirectly: map[string]struct{}{}}
	chatModelIns11, err := newChatModel(ctx)
	if err != nil {
		return nil, err
	}
	config.ToolCallingModel = chatModelIns11
	//searchTool, err := newSearchTool(ctx)
	//if err != nil {
	//	return nil, err
	//}
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorClusterOverviewTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorAnomaliesTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorPerformanceTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorTrendTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorDetailTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewGetCurrentTimeTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewQueryInternalDocsTool())

	ins, err := react.NewAgent(ctx, config)
	if err != nil {
		return nil, err
	}
	lba, err = compose.AnyLambda(ins.Generate, ins.Stream, nil, nil)
	if err != nil {
		return nil, err
	}
	return lba, nil
}
