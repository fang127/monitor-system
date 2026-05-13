package plan_execute_replan

import (
	"context"
	"fmt"

	"github.com/cloudwego/eino-examples/adk/common/prints"
	"github.com/cloudwego/eino/adk"
	"github.com/cloudwego/eino/adk/prebuilt/planexecute"
)

// BuildPlanAgent 构建一个包含计划、执行和重新计划能力的智能体，并执行给定的查询。返回最终的响应内容、详细的事件日志以及可能发生的错误。
// 这个函数关心的不只是最终回答，也收集了中间执行轨迹
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
	// 创建一个计划执行智能体，配置其使用上述的计划、执行和重新计划智能体，并设置最大迭代次数。
	planExecuteAgent, err := planexecute.New(ctx, &planexecute.Config{
		Planner:       planAgent,
		Executor:      executeAgent,
		Replanner:     replanAgent,
		MaxIterations: 20,
	})
	if err != nil {
		return "", []string{}, fmt.Errorf("build PlanExecuteAgent Error: %v", err)
	}
	// 使用构建好的计划执行智能体运行查询，并收集事件日志和最终响应。
	r := adk.NewRunner(ctx, adk.RunnerConfig{
		Agent: planExecuteAgent,
	})
	// 执行查询并迭代获取事件，记录每个事件的输出和错误信息，最终返回最后一个消息的内容、详细的事件日志以及可能发生的错误。
	iter := r.Query(ctx, query)
	var lastMessage adk.Message
	var detail []string
	var runErr error
	for {
		// 迭代获取事件，直到没有更多事件为止。对于每个事件，记录其输出和错误信息，并更新最后一个消息的内容。
		event, ok := iter.Next()
		if !ok {
			break
		}
		fmt.Println("------------- Event -------------")
		prints.Event(event) // 打印事件信息，便于调试和分析。
		if event.Err != nil {
			runErr = event.Err
			detail = append(detail, event.Err.Error()) // 将错误信息添加到事件日志中。
			continue
		}
		// 如果事件有输出，尝试获取消息内容并更新最后一个消息。如果获取消息失败，记录错误信息并继续处理下一个事件。
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
	// 检查最后一个消息是否存在。如果不存在且运行过程中发生了错误，返回错误信息；如果不存在但没有错误，返回一个新的错误信息；如果存在，返回最后一个消息的内容、事件日志和nil错误。
	if lastMessage == nil {
		if runErr != nil {
			return "", detail, runErr
		}
		return "", []string{}, fmt.Errorf("get lastMessage Error")
	}
	return lastMessage.Content, detail, nil
}
