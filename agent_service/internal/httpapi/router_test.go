package httpapi

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"monitor-system/agent_service/internal/agent"
	"monitor-system/agent_service/internal/knowledge"
	"monitor-system/agent_service/internal/model"
)

func TestChatRouteReturnsEnvelope(t *testing.T) {
	router := NewRouter(fakeAgent{})
	req := httptest.NewRequest(http.MethodPost, "/api/agent/chat", strings.NewReader(`{"Id":"web-1","Question":"hello"}`))
	req.Header.Set("Content-Type", "application/json")
	rec := httptest.NewRecorder()

	router.ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d", rec.Code)
	}
	var body envelope
	if err := json.Unmarshal(rec.Body.Bytes(), &body); err != nil {
		t.Fatal(err)
	}
	if body.Code != 0 || !strings.Contains(string(body.Data), "answer") {
		t.Fatalf("unexpected envelope: %s", rec.Body.String())
	}
}

func TestChatStreamRouteReturnsSSE(t *testing.T) {
	router := NewRouter(fakeAgent{})
	req := httptest.NewRequest(http.MethodPost, "/api/agent/chat_stream", strings.NewReader(`{"Id":"web-1","Question":"hello"}`))
	req.Header.Set("Content-Type", "application/json")
	rec := httptest.NewRecorder()

	router.ServeHTTP(rec, req)

	if got := rec.Header().Get("Content-Type"); !strings.Contains(got, "text/event-stream") {
		t.Fatalf("content-type = %q", got)
	}
	if !strings.Contains(rec.Body.String(), "event: message") || !strings.Contains(rec.Body.String(), "event: done") {
		t.Fatalf("unexpected SSE body: %s", rec.Body.String())
	}
}

type fakeAgent struct{}

func (fakeAgent) Chat(ctx context.Context, req agent.ChatRequest) (agent.ChatResponse, error) {
	return agent.ChatResponse{Answer: "hello from fake"}, nil
}

func (fakeAgent) ChatStream(ctx context.Context, req agent.ChatRequest) (<-chan model.StreamChunk, error) {
	ch := make(chan model.StreamChunk, 2)
	ch <- model.StreamChunk{Content: "hello"}
	close(ch)
	return ch, nil
}

func (fakeAgent) Upload(ctx context.Context, path string) (knowledge.IndexResult, error) {
	return knowledge.IndexResult{Source: path, Chunks: 1}, nil
}

func (fakeAgent) AIOps(ctx context.Context) (agent.AIOpsResponse, error) {
	return agent.AIOpsResponse{Result: "report", Detail: []string{"detail"}}, nil
}
