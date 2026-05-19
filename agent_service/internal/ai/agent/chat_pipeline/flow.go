package chat_pipeline

import (
	"context"
	"monitor-system/agent_service/internal/ai/tools"

	"github.com/cloudwego/eino/compose"
	"github.com/cloudwego/eino/flow/agent/react"
)

// 创建 ReAct Agent，并包装成 Graph 的 Lambda 节点

// newReactAgentLambda 创建一个基于 ReAct 智能体的 Lambda 函数，配置工具和模型。
func newReactAgentLambda(ctx context.Context) (lba *compose.Lambda, err error) {
	config := &react.AgentConfig{
		MaxStep:            25,                    // 最大步骤数，防止无限循环
		ToolReturnDirectly: map[string]struct{}{}} // 配置工具直接返回结果的列表
	// 配置工具调用模型
	chatModelIns11, err := newChatModel(ctx)
	if err != nil {
		return nil, err
	}
	config.ToolCallingModel = chatModelIns11
	//searchTool, err := newSearchTool(ctx)
	//if err != nil {
	//	return nil, err
	//}

	// 添加工具到配置中
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorClusterOverviewTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorAnomaliesTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorPerformanceTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorTrendTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorDetailTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewMonitorMysqlDetailTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewGetCurrentTimeTool())
	config.ToolsConfig.Tools = append(config.ToolsConfig.Tools, tools.NewQueryInternalDocsTool())

	// 创建 ReAct Agent 实例
	ins, err := react.NewAgent(ctx, config)
	if err != nil {
		return nil, err
	}
	// 将 ReAct Agent 包装成 Lambda 节点
	lba, err = compose.AnyLambda(ins.Generate, ins.Stream, nil, nil)
	if err != nil {
		return nil, err
	}
	return lba, nil
}
