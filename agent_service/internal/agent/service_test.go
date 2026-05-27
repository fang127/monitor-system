package agent

import (
	"context"
	"encoding/json"
	"errors"
	"strings"
	"testing"

	"monitor-system/agent_service/internal/gateway"
	"monitor-system/agent_service/internal/knowledge"
	"monitor-system/agent_service/internal/memory"
	"monitor-system/agent_service/internal/model"
)

func TestChatFallsBackWhenModelUnavailable(t *testing.T) {
	mem := memory.NewStore(true, 4)
	svc := NewService(fakeGateway{}, fakeKnowledge{}, unavailableModel{}, unavailableModel{}, mem)

	resp, err := svc.Chat(context.Background(), ChatRequest{ID: "s1", Question: "当前集群怎么样"})
	if err != nil {
		t.Fatalf("Chat returned error: %v", err)
	}
	if !strings.Contains(resp.Answer, "AI 模型未配置") {
		t.Fatalf("answer = %q", resp.Answer)
	}
	if got := mem.Get("s1"); len(got) != 2 {
		t.Fatalf("memory len = %d, want 2", len(got))
	}
}

func TestAIOpsReportUsesGatewayFacts(t *testing.T) {
	svc := NewService(fakeGateway{}, fakeKnowledge{}, unavailableModel{}, unavailableModel{}, memory.NewStore(false, 4))

	resp, err := svc.AIOps(context.Background())
	if err != nil {
		t.Fatalf("AIOps returned error: %v", err)
	}
	if !strings.Contains(resp.Result, "AI 运维分析报告") || !strings.Contains(resp.Result, "server-a") {
		t.Fatalf("unexpected report: %s", resp.Result)
	}
	if len(resp.Detail) == 0 {
		t.Fatal("expected detail entries")
	}
}

type fakeGateway struct{}

func (fakeGateway) LatestTyped(ctx context.Context) (gateway.LatestResponse, json.RawMessage, error) {
	raw := json.RawMessage(`{"servers":[{"server_name":"server-a","score":72,"cpu_percent":88},{"server_name":"server-b","score":96}],"cluster_stats":{"total_servers":2}}`)
	return gateway.LatestResponse{
		Servers:      []gateway.ServerSummary{{ServerName: "server-a", Score: 72, CPUPercent: 88}, {ServerName: "server-b", Score: 96}},
		ClusterStats: map[string]any{"total_servers": float64(2)},
	}, raw, nil
}

func (fakeGateway) Anomalies(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error) {
	return json.RawMessage(`{"anomalies":[{"server_name":"server-a","anomaly_type":"CPU","severity":"HIGH"}]}`), nil
}

func (fakeGateway) Performance(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error) {
	return json.RawMessage(`{"records":[]}`), nil
}

func (fakeGateway) Trend(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error) {
	return json.RawMessage(`{"records":[]}`), nil
}

func (fakeGateway) Detail(ctx context.Context, server, kind string, q gateway.Query) (json.RawMessage, error) {
	return json.RawMessage(`{"records":[]}`), nil
}

type fakeKnowledge struct{}

func (fakeKnowledge) IndexFile(ctx context.Context, path string) (knowledge.IndexResult, error) {
	return knowledge.IndexResult{Source: path, Chunks: 1}, nil
}

func (fakeKnowledge) Search(ctx context.Context, query string, limit int) ([]knowledge.Document, error) {
	return []knowledge.Document{{Source: "runbook.md", Content: "CPU 高时先确认进程负载和最近变更。"}}, nil
}

type unavailableModel struct{}

func (unavailableModel) Complete(ctx context.Context, req model.ChatRequest) (model.ChatResponse, error) {
	return model.ChatResponse{}, model.ErrUnavailable
}

func (unavailableModel) Stream(ctx context.Context, req model.ChatRequest) (<-chan model.StreamChunk, error) {
	return nil, model.ErrUnavailable
}

var errBoom = errors.New("boom")
