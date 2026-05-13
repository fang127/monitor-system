package main

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/ai/tools"

	"github.com/cloudwego/eino/components/tool"
)

func main() {
	ctx := context.Background()
	toolList := []tool.BaseTool{
		tools.NewMonitorClusterOverviewTool(),
		tools.NewMonitorAnomaliesTool(),
		tools.NewMonitorPerformanceTool(),
		tools.NewMonitorTrendTool(),
		tools.NewMonitorDetailTool(),
		tools.NewGetCurrentTimeTool(),
		tools.NewQueryInternalDocsTool(),
	}
	for _, todoTool := range toolList {
		info, err := todoTool.Info(ctx)
		if err != nil {
			panic(err)
		}
		fmt.Printf("%s: %s\n", info.Name, info.Desc)
	}
}
