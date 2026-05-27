package agent

import (
	"context"
	"encoding/json"
	"strings"
	"testing"

	einotool "github.com/cloudwego/eino/components/tool"

	"monitor-system/agent_service/internal/gateway"
	"monitor-system/agent_service/internal/knowledge"
)

func TestBuildMonitorToolsExposeGatewayAndDocs(t *testing.T) {
	tools := BuildMonitorTools(fakeGateway{}, fakeKnowledge{})
	names := map[string]bool{}
	for _, item := range tools {
		info, err := item.Info(context.Background())
		if err != nil {
			t.Fatalf("Info returned error: %v", err)
		}
		names[info.Name] = true
	}
	for _, name := range []string{
		"query_monitor_cluster_overview",
		"query_monitor_anomalies",
		"query_monitor_performance",
		"query_monitor_trend",
		"query_monitor_detail",
		"query_monitor_mysql_detail",
		"query_monitor_redis_detail",
		"query_internal_docs",
		"get_current_time",
	} {
		if !names[name] {
			t.Fatalf("missing tool %s in %#v", name, names)
		}
	}
}

func TestMonitorToolUsesInjectedGateway(t *testing.T) {
	tools := BuildMonitorTools(fakeGateway{}, fakeKnowledge{})
	var overview string
	for _, item := range tools {
		info, _ := item.Info(context.Background())
		if info.Name == "query_monitor_cluster_overview" {
			invokable := item.(einotool.InvokableTool)
			var err error
			overview, err = invokable.InvokableRun(context.Background(), `{}`)
			if err != nil {
				t.Fatalf("InvokableRun returned error: %v", err)
			}
			break
		}
	}
	if !strings.Contains(overview, "server-a") {
		t.Fatalf("overview output = %s", overview)
	}
}

func TestQueryInternalDocsToolUsesInjectedKnowledgeStore(t *testing.T) {
	tools := BuildMonitorTools(fakeGateway{}, fakeKnowledge{})
	var docs string
	for _, item := range tools {
		info, _ := item.Info(context.Background())
		if info.Name == "query_internal_docs" {
			invokable := item.(einotool.InvokableTool)
			var err error
			docs, err = invokable.InvokableRun(context.Background(), `{"query":"CPU"}`)
			if err != nil {
				t.Fatalf("InvokableRun returned error: %v", err)
			}
			break
		}
	}
	if !strings.Contains(docs, "CPU 高") {
		t.Fatalf("docs output = %s", docs)
	}
}

type toolGateway struct{}

func (toolGateway) LatestTyped(ctx context.Context) (gateway.LatestResponse, json.RawMessage, error) {
	return fakeGateway{}.LatestTyped(ctx)
}

func (toolGateway) Anomalies(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error) {
	return json.RawMessage(`{"anomalies":[]}`), nil
}

func (toolGateway) Performance(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error) {
	return json.RawMessage(`{"records":[]}`), nil
}

func (toolGateway) Trend(ctx context.Context, server string, q gateway.Query) (json.RawMessage, error) {
	return json.RawMessage(`{"records":[]}`), nil
}

func (toolGateway) Detail(ctx context.Context, server, kind string, q gateway.Query) (json.RawMessage, error) {
	return json.RawMessage(`{"records":[]}`), nil
}

var _ knowledge.Store = fakeKnowledge{}
