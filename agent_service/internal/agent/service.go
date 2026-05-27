package agent

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strings"
	"time"

	"github.com/cloudwego/eino/adk"
	"github.com/cloudwego/eino/adk/prebuilt/planexecute"
	einomodel "github.com/cloudwego/eino/components/model"
	einoretriever "github.com/cloudwego/eino/components/retriever"
	"github.com/cloudwego/eino/compose"
	"github.com/cloudwego/eino/flow/agent/react"
	"github.com/cloudwego/eino/schema"

	"monitor-system/agent_service/internal/gateway"
	"monitor-system/agent_service/internal/knowledge"
	"monitor-system/agent_service/internal/memory"
	localmodel "monitor-system/agent_service/internal/model"
)

type Gateway interface {
	LatestTyped(ctx context.Context) (gateway.LatestResponse, json.RawMessage, error)
	Anomalies(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error)
	Performance(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error)
	Trend(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error)
	Detail(ctx context.Context, server, kind string, q gateway.Query) (json.RawMessage, error)
}

type Service struct {
	gateway   Gateway
	store     knowledge.Store
	quick     localmodel.ChatModel
	think     localmodel.ChatModel
	quickEino einomodel.ToolCallingChatModel
	thinkEino einomodel.ToolCallingChatModel
	memory    *memory.Store
}

type ChatRequest struct {
	ID       string
	Question string
}

type ChatResponse struct {
	Answer string `json:"answer"`
}

type AIOpsResponse struct {
	Result string   `json:"result"`
	Detail []string `json:"detail"`
}

func NewService(g Gateway, store knowledge.Store, quick localmodel.ChatModel, think localmodel.ChatModel, mem *memory.Store) *Service {
	return &Service{gateway: g, store: store, quick: quick, think: think, memory: mem}
}

func NewServiceWithEino(g Gateway, store knowledge.Store, quick localmodel.ChatModel, think localmodel.ChatModel, mem *memory.Store, quickEino einomodel.ToolCallingChatModel, thinkEino einomodel.ToolCallingChatModel) *Service {
	return &Service{gateway: g, store: store, quick: quick, think: think, memory: mem, quickEino: quickEino, thinkEino: thinkEino}
}

func (s *Service) Chat(ctx context.Context, req ChatRequest) (ChatResponse, error) {
	question := strings.TrimSpace(req.Question)
	if question == "" {
		return ChatResponse{}, fmt.Errorf("Question is required")
	}
	sessionID := strings.TrimSpace(req.ID)
	if sessionID == "" {
		sessionID = "anonymous"
	}
	pipeline, err := s.buildChatPipeline(ctx)
	if err != nil {
		return ChatResponse{}, err
	}
	out, err := pipeline.Invoke(ctx, &chatPipelineInput{SessionID: sessionID, Question: question})
	if err != nil {
		return ChatResponse{}, err
	}
	return ChatResponse{Answer: out.Answer}, nil
}

func (s *Service) ChatStream(ctx context.Context, req ChatRequest) (<-chan localmodel.StreamChunk, error) {
	question := strings.TrimSpace(req.Question)
	if question == "" {
		return nil, fmt.Errorf("Question is required")
	}
	sessionID := strings.TrimSpace(req.ID)
	if sessionID == "" {
		sessionID = "anonymous"
	}
	if s.quickEino != nil {
		return s.reactChatStream(ctx, &chatPipelineInput{SessionID: sessionID, Question: question})
	}
	pipeline, err := s.buildChatPipeline(ctx)
	if err != nil {
		return nil, err
	}
	out, err := pipeline.Invoke(ctx, &chatPipelineInput{SessionID: sessionID, Question: question})
	if err != nil {
		return nil, err
	}
	return mustStream(localmodel.Fallback(out.Answer), ctx), nil
}

func (s *Service) Upload(ctx context.Context, path string) (knowledge.IndexResult, error) {
	if s.store == nil {
		return knowledge.IndexResult{}, fmt.Errorf("knowledge store is not configured")
	}
	return s.store.IndexFile(ctx, path)
}

func (s *Service) AIOps(ctx context.Context) (AIOpsResponse, error) {
	pipeline, err := s.buildAIOpsPipeline(ctx)
	if err != nil {
		return AIOpsResponse{}, err
	}
	out, err := pipeline.Invoke(ctx, &aiOpsPipelineInput{})
	if err != nil {
		return AIOpsResponse{}, err
	}
	return AIOpsResponse{Result: out.Result, Detail: out.Detail}, nil
}

type chatPipelineInput struct {
	SessionID string
	Question  string
}

type chatContextOutput struct {
	SessionID   string
	Question    string
	ContextText string
	Detail      []string
}

type chatPipelineOutput struct {
	Answer string
}

type aiOpsPipelineInput struct{}

type aiOpsFacts struct {
	Latest gateway.LatestResponse
	Detail []string
	Error  string
}

type aiOpsPipelineOutput struct {
	Result string
	Detail []string
}

func (s *Service) buildChatPipeline(ctx context.Context) (compose.Runnable[*chatPipelineInput, *chatPipelineOutput], error) {
	if s.quickEino != nil {
		return s.buildReactChatPipeline(ctx)
	}
	g := compose.NewGraph[*chatPipelineInput, *chatPipelineOutput]()
	contextNode := compose.InvokableLambdaWithOption(func(ctx context.Context, input *chatPipelineInput, opts ...any) (*chatContextOutput, error) {
		return s.collectChatContextNode(ctx, input)
	})
	generateNode := compose.InvokableLambdaWithOption(func(ctx context.Context, input *chatContextOutput, opts ...any) (*chatPipelineOutput, error) {
		return s.generateChatNode(ctx, input)
	})
	if err := g.AddLambdaNode("CollectContext", contextNode, compose.WithNodeName("EinoChatCollectContext")); err != nil {
		return nil, err
	}
	if err := g.AddLambdaNode("GenerateAnswer", generateNode, compose.WithNodeName("EinoChatGenerateAnswer")); err != nil {
		return nil, err
	}
	if err := g.AddEdge(compose.START, "CollectContext"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("CollectContext", "GenerateAnswer"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("GenerateAnswer", compose.END); err != nil {
		return nil, err
	}
	return g.Compile(ctx, compose.WithGraphName("MonitorSystemChatAgent"))
}

func (s *Service) buildAIOpsPipeline(ctx context.Context) (compose.Runnable[*aiOpsPipelineInput, *aiOpsPipelineOutput], error) {
	if s.quickEino != nil && s.thinkEino != nil {
		g := compose.NewGraph[*aiOpsPipelineInput, *aiOpsPipelineOutput]()
		adkNode := compose.InvokableLambdaWithOption(func(ctx context.Context, input *aiOpsPipelineInput, opts ...any) (*aiOpsPipelineOutput, error) {
			return s.runPlanExecuteReplan(ctx)
		})
		if err := g.AddLambdaNode("PlanExecuteReplan", adkNode, compose.WithNodeName("EinoAIOpsPlanExecuteReplan")); err != nil {
			return nil, err
		}
		if err := g.AddEdge(compose.START, "PlanExecuteReplan"); err != nil {
			return nil, err
		}
		if err := g.AddEdge("PlanExecuteReplan", compose.END); err != nil {
			return nil, err
		}
		return g.Compile(ctx, compose.WithGraphName("MonitorSystemAIOpsPlanExecuteReplanAgent"))
	}

	g := compose.NewGraph[*aiOpsPipelineInput, *aiOpsPipelineOutput]()
	collectNode := compose.InvokableLambdaWithOption(func(ctx context.Context, input *aiOpsPipelineInput, opts ...any) (*aiOpsFacts, error) {
		return s.collectAIOpsFactsNode(ctx)
	})
	reportNode := compose.InvokableLambdaWithOption(func(ctx context.Context, input *aiOpsFacts, opts ...any) (*aiOpsPipelineOutput, error) {
		return s.generateAIOpsReportNode(ctx, input)
	})
	if err := g.AddLambdaNode("CollectFacts", collectNode, compose.WithNodeName("EinoAIOpsCollectFacts")); err != nil {
		return nil, err
	}
	if err := g.AddLambdaNode("GenerateReport", reportNode, compose.WithNodeName("EinoAIOpsGenerateReport")); err != nil {
		return nil, err
	}
	if err := g.AddEdge(compose.START, "CollectFacts"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("CollectFacts", "GenerateReport"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("GenerateReport", compose.END); err != nil {
		return nil, err
	}
	return g.Compile(ctx, compose.WithGraphName("MonitorSystemAIOpsAgent"))
}

func (s *Service) buildReactChatPipeline(ctx context.Context) (compose.Runnable[*chatPipelineInput, *chatPipelineOutput], error) {
	g := compose.NewGraph[*chatPipelineInput, *chatPipelineOutput]()
	prepareNode := compose.InvokableLambdaWithOption(func(ctx context.Context, input *chatPipelineInput, opts ...any) ([]*schema.Message, error) {
		return s.prepareReactMessages(ctx, input)
	})
	reactNode, err := s.newReactAgentLambda(ctx)
	if err != nil {
		return nil, err
	}
	outputNode := compose.InvokableLambdaWithOption(func(ctx context.Context, msg *schema.Message, opts ...any) (*chatPipelineOutput, error) {
		answer := strings.TrimSpace(msg.Content)
		return &chatPipelineOutput{Answer: answer}, nil
	})
	if err := g.AddLambdaNode("PrepareMessages", prepareNode, compose.WithNodeName("EinoChatPrepareMessages")); err != nil {
		return nil, err
	}
	if err := g.AddLambdaNode("ReActAgent", reactNode, compose.WithNodeName("EinoChatReActAgent")); err != nil {
		return nil, err
	}
	if err := g.AddLambdaNode("Output", outputNode, compose.WithNodeName("EinoChatOutput")); err != nil {
		return nil, err
	}
	if err := g.AddEdge(compose.START, "PrepareMessages"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("PrepareMessages", "ReActAgent"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("ReActAgent", "Output"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("Output", compose.END); err != nil {
		return nil, err
	}
	return g.Compile(ctx, compose.WithGraphName("MonitorSystemReActChatAgent"))
}

func (s *Service) buildReactChatRunnable(ctx context.Context) (compose.Runnable[*chatPipelineInput, *schema.Message], error) {
	g := compose.NewGraph[*chatPipelineInput, *schema.Message]()
	prepareNode := compose.InvokableLambdaWithOption(func(ctx context.Context, input *chatPipelineInput, opts ...any) ([]*schema.Message, error) {
		return s.prepareReactMessages(ctx, input)
	})
	reactNode, err := s.newReactAgentLambda(ctx)
	if err != nil {
		return nil, err
	}
	if err := g.AddLambdaNode("PrepareMessages", prepareNode, compose.WithNodeName("EinoChatPrepareMessages")); err != nil {
		return nil, err
	}
	if err := g.AddLambdaNode("ReActAgent", reactNode, compose.WithNodeName("EinoChatReActAgent")); err != nil {
		return nil, err
	}
	if err := g.AddEdge(compose.START, "PrepareMessages"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("PrepareMessages", "ReActAgent"); err != nil {
		return nil, err
	}
	if err := g.AddEdge("ReActAgent", compose.END); err != nil {
		return nil, err
	}
	return g.Compile(ctx, compose.WithGraphName("MonitorSystemReActChatStreamAgent"))
}

func (s *Service) newReactAgentLambda(ctx context.Context) (*compose.Lambda, error) {
	agent, err := react.NewAgent(ctx, &react.AgentConfig{
		ToolCallingModel: s.quickEino,
		ToolsConfig: compose.ToolsNodeConfig{
			Tools: BuildMonitorTools(s.gateway, s.store),
		},
		MaxStep:            25,
		ToolReturnDirectly: map[string]struct{}{},
		GraphName:          "MonitorSystemReActAgent",
	})
	if err != nil {
		return nil, err
	}
	return compose.AnyLambda(agent.Generate, agent.Stream, nil, nil)
}

func (s *Service) prepareReactMessages(ctx context.Context, input *chatPipelineInput) ([]*schema.Message, error) {
	contextText, _ := s.chatContext(ctx, input.Question)
	if retriever := retrieverForStore(s.store); retriever != nil {
		if docs, err := retriever.Retrieve(ctx, input.Question); err == nil && len(docs) > 0 {
			converted := knowledge.DocumentsFromSchema(docs)
			var parts []string
			for _, doc := range converted {
				parts = append(parts, fmt.Sprintf("文档 %s: %s", doc.Source, doc.Content))
			}
			if len(parts) > 0 {
				if contextText != "" {
					contextText += "\n"
				}
				contextText += strings.Join(parts, "\n")
			}
		}
	}
	messages := []*schema.Message{
		schema.SystemMessage("你是 monitor_system 的 AI 运维助手。必须优先使用可用工具查询监控事实；不要编造数据；只通过 api_gateway 工具获取监控信息。当前时间: " + time.Now().Format("2006-01-02 15:04:05")),
	}
	for _, msg := range s.memory.Get(input.SessionID) {
		switch msg.Role {
		case "assistant":
			messages = append(messages, schema.AssistantMessage(msg.Content, nil))
		default:
			messages = append(messages, schema.UserMessage(msg.Content))
		}
	}
	if contextText != "" {
		messages = append(messages, schema.SystemMessage("相关知识库与上下文:\n"+contextText))
	}
	messages = append(messages, schema.UserMessage(input.Question))
	return messages, nil
}

func (s *Service) reactChatStream(ctx context.Context, input *chatPipelineInput) (<-chan localmodel.StreamChunk, error) {
	runnable, err := s.buildReactChatRunnable(ctx)
	if err != nil {
		return nil, err
	}
	reader, err := runnable.Stream(ctx, input)
	if err != nil {
		return nil, err
	}
	out := make(chan localmodel.StreamChunk)
	go func() {
		defer close(out)
		defer reader.Close()
		var answer strings.Builder
		for {
			msg, err := reader.Recv()
			if errors.Is(err, io.EOF) {
				if answer.Len() > 0 {
					s.memory.Append(input.SessionID, memory.Message{Role: "user", Content: input.Question})
					s.memory.Append(input.SessionID, memory.Message{Role: "assistant", Content: answer.String()})
				}
				return
			}
			if err != nil {
				out <- localmodel.StreamChunk{Err: err}
				return
			}
			if msg == nil || msg.Content == "" {
				continue
			}
			answer.WriteString(msg.Content)
			out <- localmodel.StreamChunk{Content: msg.Content}
		}
	}()
	return out, nil
}

func retrieverForStore(store knowledge.Store) einoretriever.Retriever {
	if store == nil {
		return nil
	}
	if einoStore, ok := store.(knowledge.EinoStore); ok {
		return einoStore.Retriever()
	}
	return nil
}

func (s *Service) collectChatContextNode(ctx context.Context, input *chatPipelineInput) (*chatContextOutput, error) {
	contextText, detail := s.chatContext(ctx, input.Question)
	return &chatContextOutput{
		SessionID:   input.SessionID,
		Question:    input.Question,
		ContextText: contextText,
		Detail:      detail,
	}, nil
}

func (s *Service) generateChatNode(ctx context.Context, input *chatContextOutput) (*chatPipelineOutput, error) {
	messages := s.modelMessages(input.SessionID, input.Question, input.ContextText)
	resp, err := s.quick.Complete(ctx, localmodel.ChatRequest{Messages: messages, Temperature: 0.2})
	answer := strings.TrimSpace(resp.Content)
	if err != nil {
		if !errors.Is(err, localmodel.ErrUnavailable) {
			input.Detail = append(input.Detail, "模型调用失败: "+err.Error())
		}
		answer = fallbackChatAnswer(input.Question, input.ContextText, input.Detail)
	}
	s.memory.Append(input.SessionID, memory.Message{Role: "user", Content: input.Question})
	s.memory.Append(input.SessionID, memory.Message{Role: "assistant", Content: answer})
	return &chatPipelineOutput{Answer: answer}, nil
}

func (s *Service) collectAIOpsFactsNode(ctx context.Context) (*aiOpsFacts, error) {
	latest, rawLatest, err := s.gateway.LatestTyped(ctx)
	var detail []string
	if err != nil {
		return &aiOpsFacts{Error: err.Error(), Detail: []string{err.Error()}}, nil
	}
	detail = append(detail, "cluster_overview: "+compact(rawLatest))

	servers := selectServers(latest.Servers)
	for _, server := range servers {
		raw, err := s.gateway.Anomalies(ctx, server.ServerName, gateway.Query{Page: 1, PageSize: 20})
		if err != nil {
			detail = append(detail, "anomalies["+server.ServerName+"]: "+err.Error())
			continue
		}
		detail = append(detail, "anomalies["+server.ServerName+"]: "+compact(raw))
		lowerAnomaly := strings.ToLower(string(raw))
		if strings.Contains(lowerAnomaly, "mysql") {
			if raw, err := s.gateway.Detail(ctx, server.ServerName, "mysql", gateway.Query{Page: 1, PageSize: 20}); err == nil {
				detail = append(detail, "mysql_detail["+server.ServerName+"]: "+compact(raw))
			}
		}
		if strings.Contains(lowerAnomaly, "redis") {
			if raw, err := s.gateway.Detail(ctx, server.ServerName, "redis", gateway.Query{Page: 1, PageSize: 20}); err == nil {
				detail = append(detail, "redis_detail["+server.ServerName+"]: "+compact(raw))
			}
		}
		if server.Score > 0 && server.Score < 80 {
			if raw, err := s.gateway.Detail(ctx, server.ServerName, "mem", gateway.Query{Page: 1, PageSize: 20}); err == nil {
				detail = append(detail, "mem_detail["+server.ServerName+"]: "+compact(raw))
			}
		}
	}
	if s.store != nil {
		docs, _ := s.store.Search(ctx, "monitor_system AI 运维 异常 处理手册", 3)
		for _, doc := range docs {
			detail = append(detail, "doc["+doc.Source+"]: "+doc.Content)
		}
	}
	return &aiOpsFacts{Latest: latest, Detail: detail}, nil
}

func (s *Service) generateAIOpsReportNode(ctx context.Context, facts *aiOpsFacts) (*aiOpsPipelineOutput, error) {
	if facts.Error != "" {
		report := "AI 运维分析报告\n---\n# 集群健康概览\n无法通过 api_gateway 获取集群概览: " + facts.Error + "\n# 结论\n请先确认 api_gateway 和 Manager 是否可用。"
		return &aiOpsPipelineOutput{Result: report, Detail: facts.Detail}, nil
	}

	prompt := aiOpsPrompt(facts.Latest, facts.Detail)
	resp, err := s.think.Complete(ctx, localmodel.ChatRequest{Messages: []localmodel.Message{
		{Role: "system", Content: "你是 monitor_system 的 AI 运维分析助手，只能基于给定事实输出中文报告。"},
		{Role: "user", Content: prompt},
	}, Temperature: 0.1})
	if err == nil && strings.TrimSpace(resp.Content) != "" {
		return &aiOpsPipelineOutput{Result: resp.Content, Detail: facts.Detail}, nil
	}
	return &aiOpsPipelineOutput{Result: deterministicReport(facts.Latest, facts.Detail), Detail: facts.Detail}, nil
}

func (s *Service) runPlanExecuteReplan(ctx context.Context) (*aiOpsPipelineOutput, error) {
	tools := BuildMonitorTools(s.gateway, s.store)
	planner, err := planexecute.NewPlanner(ctx, &planexecute.PlannerConfig{
		ToolCallingChatModel: s.thinkEino,
	})
	if err != nil {
		return s.fallbackAIOpsWithDetail(ctx, "ADK planner 初始化失败: "+err.Error())
	}
	executor, err := planexecute.NewExecutor(ctx, &planexecute.ExecutorConfig{
		Model: s.quickEino,
		ToolsConfig: adk.ToolsConfig{
			ToolsNodeConfig: compose.ToolsNodeConfig{Tools: tools},
		},
		MaxIterations: 20,
	})
	if err != nil {
		return s.fallbackAIOpsWithDetail(ctx, "ADK executor 初始化失败: "+err.Error())
	}
	replanner, err := planexecute.NewReplanner(ctx, &planexecute.ReplannerConfig{
		ChatModel: s.thinkEino,
	})
	if err != nil {
		return s.fallbackAIOpsWithDetail(ctx, "ADK replanner 初始化失败: "+err.Error())
	}
	agent, err := planexecute.New(ctx, &planexecute.Config{
		Planner:       planner,
		Executor:      executor,
		Replanner:     replanner,
		MaxIterations: 20,
	})
	if err != nil {
		return s.fallbackAIOpsWithDetail(ctx, "ADK plan-execute-replan 初始化失败: "+err.Error())
	}

	runner := adk.NewRunner(ctx, adk.RunnerConfig{Agent: agent})
	iter := runner.Query(ctx, aiOpsInstruction())

	var detail []string
	var lastMessage adk.Message
	var runErr error
	for {
		event, ok := iter.Next()
		if !ok {
			break
		}
		if event == nil {
			continue
		}
		if event.Err != nil {
			runErr = event.Err
			detail = append(detail, "adk_error: "+event.Err.Error())
			continue
		}
		msg, _, err := adk.GetMessage(event)
		if err != nil {
			runErr = err
			detail = append(detail, "adk_message_error: "+err.Error())
			continue
		}
		if msg == nil {
			continue
		}
		lastMessage = msg
		if strings.TrimSpace(msg.Content) != "" {
			detail = append(detail, fmt.Sprintf("%s: %s", event.AgentName, strings.TrimSpace(msg.Content)))
		}
	}
	if runErr != nil && lastMessage == nil {
		out, _ := s.fallbackAIOpsWithDetail(ctx, "ADK 执行失败: "+runErr.Error())
		out.Detail = append(detail, out.Detail...)
		return out, nil
	}
	if lastMessage == nil || strings.TrimSpace(lastMessage.Content) == "" {
		out, _ := s.fallbackAIOpsWithDetail(ctx, "ADK 未返回最终报告")
		out.Detail = append(detail, out.Detail...)
		return out, nil
	}
	return &aiOpsPipelineOutput{Result: lastMessage.Content, Detail: detail}, nil
}

func (s *Service) fallbackAIOpsWithDetail(ctx context.Context, reason string) (*aiOpsPipelineOutput, error) {
	facts, err := s.collectAIOpsFactsNode(ctx)
	if err != nil {
		return nil, err
	}
	facts.Detail = append([]string{reason}, facts.Detail...)
	return s.generateAIOpsReportNode(ctx, facts)
}

func (s *Service) chatContext(ctx context.Context, question string) (string, []string) {
	var parts []string
	var detail []string
	if s.gateway != nil && looksOperational(question) {
		if _, raw, err := s.gateway.LatestTyped(ctx); err == nil {
			text := "监控概览: " + compact(raw)
			parts = append(parts, text)
			detail = append(detail, text)
		} else {
			detail = append(detail, "监控概览获取失败: "+err.Error())
		}
	}
	if s.store != nil {
		if docs, err := s.store.Search(ctx, question, 3); err == nil {
			for _, doc := range docs {
				text := fmt.Sprintf("文档 %s: %s", doc.Source, doc.Content)
				parts = append(parts, text)
				detail = append(detail, text)
			}
		}
	}
	return strings.Join(parts, "\n"), detail
}

func (s *Service) modelMessages(sessionID, question, contextText string) []localmodel.Message {
	messages := []localmodel.Message{{
		Role: "system",
		Content: "你是 monitor_system 的 AI 运维助手。不要编造监控数据；如果事实不足，请明确说明。当前时间: " +
			time.Now().Format("2006-01-02 15:04:05"),
	}}
	for _, msg := range s.memory.Get(sessionID) {
		messages = append(messages, localmodel.Message{Role: msg.Role, Content: msg.Content})
	}
	if contextText != "" {
		messages = append(messages, localmodel.Message{Role: "system", Content: "可用上下文:\n" + contextText})
	}
	messages = append(messages, localmodel.Message{Role: "user", Content: question})
	return messages
}

func mustStream(m localmodel.ChatModel, ctx context.Context) <-chan localmodel.StreamChunk {
	ch, _ := m.Stream(ctx, localmodel.ChatRequest{})
	return ch
}

func looksOperational(question string) bool {
	q := strings.ToLower(question)
	keywords := []string{"集群", "服务器", "异常", "cpu", "内存", "磁盘", "redis", "mysql", "健康", "趋势", "告警"}
	for _, keyword := range keywords {
		if strings.Contains(q, keyword) {
			return true
		}
	}
	return false
}

func fallbackChatAnswer(question string, contextText string, detail []string) string {
	var b strings.Builder
	b.WriteString("AI 模型未配置或暂不可用，以下是基于当前可用事实的摘要。\n")
	b.WriteString("问题: " + question + "\n")
	if contextText != "" {
		b.WriteString("可用上下文:\n" + contextText + "\n")
	} else if len(detail) > 0 {
		b.WriteString("可用上下文:\n" + strings.Join(detail, "\n") + "\n")
	} else {
		b.WriteString("暂无可用监控或知识库上下文。请确认 api_gateway、文档索引或模型配置。\n")
	}
	return b.String()
}

func selectServers(servers []gateway.ServerSummary) []gateway.ServerSummary {
	var selected []gateway.ServerSummary
	for _, server := range servers {
		if len(selected) < 3 && (server.Score == 0 || server.Score < 85 || selected == nil) {
			selected = append(selected, server)
		}
	}
	if len(selected) == 0 && len(servers) > 0 {
		selected = append(selected, servers[0])
	}
	return selected
}

func aiOpsPrompt(latest gateway.LatestResponse, detail []string) string {
	raw, _ := json.Marshal(latest)
	return "请基于以下集群概览和查询细节生成 AI 运维分析报告。\n集群概览: " + string(raw) + "\n细节:\n" + strings.Join(detail, "\n")
}

func aiOpsInstruction() string {
	return strings.Join([]string{
		"请作为 monitor_system 的 AI 运维专家执行一次完整巡检。",
		"必须先制定计划，再使用可用工具查询监控事实和内部文档，最后根据执行结果决定是否需要重规划。",
		"只能通过 api_gateway 工具获取监控事实，不要直连 Manager、MySQL 或 Redis。",
		"优先查询集群概览，再针对低分、异常、MySQL、Redis、内存、磁盘或趋势问题补充明细。",
		"输出中文报告，包含: 集群健康概览、异常与低分服务器清单、根因分析、处理建议、需要人工确认的事项、结论。",
		"如果依赖不可用，请明确写出不可用原因，并基于已获得事实给出保守建议。",
	}, "\n")
}

func deterministicReport(latest gateway.LatestResponse, detail []string) string {
	var b strings.Builder
	b.WriteString("AI 运维分析报告\n---\n")
	b.WriteString("# 集群健康概览\n")
	if len(latest.Servers) == 0 {
		b.WriteString("当前没有从 api_gateway 获取到服务器列表。\n")
	} else {
		b.WriteString(fmt.Sprintf("共获取 %d 台服务器的最新状态。\n", len(latest.Servers)))
		for _, server := range latest.Servers {
			b.WriteString(fmt.Sprintf("- %s: score=%.1f cpu=%.1f mem=%.1f disk=%.1f\n", server.ServerName, server.Score, server.CPUPercent, server.MemUsedPercent, server.DiskUtilPercent))
		}
	}
	b.WriteString("# 异常与低分服务器清单\n")
	for _, server := range latest.Servers {
		if server.Score > 0 && server.Score < 85 {
			b.WriteString(fmt.Sprintf("- %s 评分偏低: %.1f\n", server.ServerName, server.Score))
		}
	}
	b.WriteString("# 根因分析\n")
	b.WriteString("模型未配置，已收集监控事实但未进行大模型推理。请结合 detail 中的异常、趋势和明细记录进一步确认。\n")
	b.WriteString("# 处理建议\n")
	b.WriteString("- 优先检查评分低或异常严重的服务器。\n- 对 CPU、内存、磁盘、MySQL、Redis 指标分别确认最近变更和资源压力。\n")
	b.WriteString("# 需要人工确认的事项\n")
	b.WriteString("- api_gateway 和 Manager 是否持续可用。\n- 是否已配置 AI 模型和知识库索引。\n")
	b.WriteString("## 结论\n")
	if len(detail) > 0 {
		b.WriteString("已完成基础事实收集，建议按 detail 中的服务器顺序排查。\n")
	} else {
		b.WriteString("当前事实不足，请先恢复监控查询链路。\n")
	}
	return b.String()
}

func compact(raw json.RawMessage) string {
	text := strings.TrimSpace(string(raw))
	if len(text) > 1200 {
		return text[:1200] + "...(truncated)"
	}
	return text
}
