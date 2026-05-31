package agent

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"monitor-system/agent_service/internal/ai/pipeline/chat"
	"monitor-system/agent_service/internal/session/memory"

	"github.com/cloudwego/eino/compose"
	"github.com/cloudwego/eino/schema"
	"github.com/gin-gonic/gin"
)

type fakeChatRunner struct {
	response string
	inputs   []*chat.UserMessage
}

func (f *fakeChatRunner) Invoke(ctx context.Context, input *chat.UserMessage, opts ...compose.Option) (*schema.Message, error) {
	_ = ctx
	_ = opts
	f.inputs = append(f.inputs, input)
	return schema.AssistantMessage(f.response, nil), nil
}

func (f *fakeChatRunner) Stream(ctx context.Context, input *chat.UserMessage, opts ...compose.Option) (*schema.StreamReader[*schema.Message], error) {
	_ = ctx
	_ = opts
	f.inputs = append(f.inputs, input)
	return schema.StreamReaderFromArray([]*schema.Message{schema.AssistantMessage(f.response, nil)}), nil
}

func (f *fakeChatRunner) Collect(ctx context.Context, input *schema.StreamReader[*chat.UserMessage], opts ...compose.Option) (*schema.Message, error) {
	_ = ctx
	_ = input
	_ = opts
	return schema.AssistantMessage(f.response, nil), nil
}

func (f *fakeChatRunner) Transform(ctx context.Context, input *schema.StreamReader[*chat.UserMessage], opts ...compose.Option) (*schema.StreamReader[*schema.Message], error) {
	_ = ctx
	_ = input
	_ = opts
	return schema.StreamReaderFromArray([]*schema.Message{schema.AssistantMessage(f.response, nil)}), nil
}

func TestChatUsesMemoryManagerWhenLongTermDisabled(t *testing.T) {
	gin.SetMode(gin.TestMode)
	manager := memory.NewInMemoryManager(memory.DefaultConfig())
	runner := &fakeChatRunner{response: "测试回答"}
	controller := NewV1WithMemoryManager(manager)
	controller.chatBuilder = func(ctx context.Context) (compose.Runnable[*chat.UserMessage, *schema.Message], error) {
		return runner, nil
	}
	router := gin.New()
	controller.RegisterRoutes(router.Group("/api/agent"))

	body := bytes.NewBufferString(`{"Id":"s1","TenantId":"tenant-a","TeamId":"team-a","ClusterId":"cluster-a","Question":"当前状态如何"}`)
	req := httptest.NewRequest(http.MethodPost, "/api/agent/chat", body)
	req.Header.Set("Content-Type", "application/json")
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, req)

	if recorder.Code != http.StatusOK {
		t.Fatalf("普通聊天状态码=%d", recorder.Code)
	}
	if len(runner.inputs) != 1 {
		t.Fatalf("fake runner 应被调用一次，got=%d", len(runner.inputs))
	}
	memCtx, err := manager.LoadContext(context.Background(), memory.MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a", SessionID: "s1"}, "继续")
	if err != nil {
		t.Fatalf("加载聊天记忆失败: %v", err)
	}
	if len(memCtx.RecentMessages) != 2 {
		t.Fatalf("聊天完成后应追加一轮消息，got=%d", len(memCtx.RecentMessages))
	}
	if len(memCtx.LongTerm) != 0 {
		t.Fatalf("长期记忆关闭时不应召回，got=%d", len(memCtx.LongTerm))
	}
}

func TestChatExtractsLongTermMemoryWhenEnabled(t *testing.T) {
	gin.SetMode(gin.TestMode)
	manager := memory.NewInMemoryManager(memory.DefaultConfig())
	scope := memory.MemoryScope{TenantID: "tenant-a", TeamID: "team-a", ClusterID: "cluster-a", SessionID: "s1"}
	if err := manager.SetLongTermEnabled(context.Background(), scope, true); err != nil {
		t.Fatalf("启用长期记忆失败: %v", err)
	}
	runner := &fakeChatRunner{response: "已记住"}
	controller := NewV1WithMemoryManager(manager)
	controller.chatBuilder = func(ctx context.Context) (compose.Runnable[*chat.UserMessage, *schema.Message], error) {
		return runner, nil
	}
	router := gin.New()
	controller.RegisterRoutes(router.Group("/api/agent"))

	body := bytes.NewBufferString(`{"Id":"s1","TenantId":"tenant-a","TeamId":"team-a","ClusterId":"cluster-a","Question":"请记住：cluster-a 集群默认先检查 Redis 慢日志"}`)
	req := httptest.NewRequest(http.MethodPost, "/api/agent/chat", body)
	req.Header.Set("Content-Type", "application/json")
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, req)

	if recorder.Code != http.StatusOK {
		t.Fatalf("普通聊天状态码=%d", recorder.Code)
	}
	items, err := manager.ListMemories(context.Background(), memory.MemorySelector{Scope: scope})
	if err != nil {
		t.Fatalf("查询长期记忆失败: %v", err)
	}
	if len(items) != 1 {
		t.Fatalf("长期记忆启用时应抽取一条记忆，got=%d", len(items))
	}
}

func TestChatStreamAppendsMemory(t *testing.T) {
	gin.SetMode(gin.TestMode)
	manager := memory.NewInMemoryManager(memory.DefaultConfig())
	runner := &fakeChatRunner{response: "流式回答"}
	controller := NewV1WithMemoryManager(manager)
	controller.chatBuilder = func(ctx context.Context) (compose.Runnable[*chat.UserMessage, *schema.Message], error) {
		return runner, nil
	}
	router := gin.New()
	controller.RegisterRoutes(router.Group("/api/agent"))

	body := bytes.NewBufferString(`{"Id":"stream-1","TenantId":"tenant-a","TeamId":"team-a","Question":"流式问题"}`)
	req := httptest.NewRequest(http.MethodPost, "/api/agent/chat_stream", body)
	req.Header.Set("Content-Type", "application/json")
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, req)

	if recorder.Code != http.StatusOK {
		t.Fatalf("流式聊天状态码=%d body=%s", recorder.Code, recorder.Body.String())
	}
	var res struct {
		Code int `json:"code"`
	}
	_ = json.Unmarshal(recorder.Body.Bytes(), &res)
	memCtx, err := manager.LoadContext(context.Background(), memory.MemoryScope{TenantID: "tenant-a", TeamID: "team-a", SessionID: "stream-1"}, "继续")
	if err != nil {
		t.Fatalf("加载流式聊天记忆失败: %v", err)
	}
	if len(memCtx.RecentMessages) != 2 {
		t.Fatalf("流式聊天完成后应追加一轮消息，got=%d", len(memCtx.RecentMessages))
	}
}

func TestChatRejectsMissingMemoryScope(t *testing.T) {
	gin.SetMode(gin.TestMode)
	manager := memory.NewInMemoryManager(memory.DefaultConfig())
	runner := &fakeChatRunner{response: "不应调用"}
	controller := NewV1WithMemoryManager(manager)
	controller.chatBuilder = func(ctx context.Context) (compose.Runnable[*chat.UserMessage, *schema.Message], error) {
		return runner, nil
	}
	router := gin.New()
	controller.RegisterRoutes(router.Group("/api/agent"))

	body := bytes.NewBufferString(`{"Id":"s1","TeamId":"team-a","Question":"当前状态如何"}`)
	req := httptest.NewRequest(http.MethodPost, "/api/agent/chat", body)
	req.Header.Set("Content-Type", "application/json")
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, req)

	var res struct {
		Code    int    `json:"code"`
		Message string `json:"message"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &res); err != nil {
		t.Fatalf("解析响应失败: %v", err)
	}
	if res.Code == 0 || res.Message != "租户 ID 不能为空" {
		t.Fatalf("缺少租户时应返回错误，body=%s", recorder.Body.String())
	}
	if len(runner.inputs) != 0 {
		t.Fatalf("作用域无效时不应调用模型，got=%d", len(runner.inputs))
	}
}
