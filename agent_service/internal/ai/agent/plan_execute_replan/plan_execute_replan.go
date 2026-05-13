package plan_execute_replan

import (
	"context"
	"fmt"

	"github.com/cloudwego/eino-examples/adk/common/prints"
	"github.com/cloudwego/eino/adk"
	"github.com/cloudwego/eino/adk/prebuilt/planexecute"
)

// BuildPlanAgent 构建一个包含计划、执行和重新计划能力的智能体，并执行给定的查询。返回最终的响应内容、详细的事件日志以及可能发生的错误。
func BuildPlanAgent(ctx context.Context, query string) (string, []string, error) {
	// 构建计划智能体、执行智能体和重新计划智能体，并将它们组合成一个计划执行智能体。
	planAgent, err := NewPlanner(ctx)
	if err != nil {
		return "", []string{}, err
	}
	executeAgent, err := NewExecutor(ctx)
	if err != nil {
		return "", []string{}, err
	}
	replanAgent, err := NewRePlanAgent(ctx)
	if err != nil {
		return "", []string{}, err
	}
	planExecuteAgent, err := planexecute.New(ctx, &planexecute.Config{
		Planner:       planAgent,
		Executor:      executeAgent,
		Replanner:     replanAgent,
		MaxIterations: 20,
	})
	if err != nil {
		return "", []string{}, fmt.Errorf("build PlanExecuteAgent Error: %v", err)
	}
	r := adk.NewRunner(ctx, adk.RunnerConfig{
		Agent: planExecuteAgent,
	})
	iter := r.Query(ctx, query)
	var lastMessage adk.Message
	var detail []string
	var runErr error
	for {
		event, ok := iter.Next()
		if !ok {
			break
		}
		fmt.Println("------------- Event -------------")
		prints.Event(event)
		if event.Err != nil {
			runErr = event.Err
			detail = append(detail, event.Err.Error())
			continue
		}
		if event.Output != nil {
			lastMessage, _, err = adk.GetMessage(event)
			if err != nil {
				runErr = err
				detail = append(detail, err.Error())
				continue
			}
			detail = append(detail, lastMessage.String())
		}
	}
	if lastMessage == nil {
		if runErr != nil {
			return "", detail, runErr
		}
		return "", []string{}, fmt.Errorf("get lastMessage Error")
	}
	return lastMessage.Content, detail, nil
}
