package agent

import (
	"context"
	"strings"
	"testing"

	"monitor-system/agent_service/internal/memory"
)

func TestChatPipelineRunsThroughEinoGraph(t *testing.T) {
	svc := NewService(fakeGateway{}, fakeKnowledge{}, unavailableModel{}, unavailableModel{}, memory.NewStore(true, 4))
	pipeline, err := svc.buildChatPipeline(context.Background())
	if err != nil {
		t.Fatalf("buildChatPipeline returned error: %v", err)
	}

	resp, err := pipeline.Invoke(context.Background(), &chatPipelineInput{
		SessionID: "s1",
		Question:  "当前集群状态怎么样",
	})
	if err != nil {
		t.Fatalf("pipeline Invoke returned error: %v", err)
	}
	if !strings.Contains(resp.Answer, "AI 模型未配置") {
		t.Fatalf("answer = %q", resp.Answer)
	}
}
