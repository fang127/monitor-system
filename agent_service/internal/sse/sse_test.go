package sse

import (
	"context"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestCreateSetsHeadersAndWritesConnectedEvent(t *testing.T) {
	service := New()
	recorder := httptest.NewRecorder()

	client, err := service.Create(context.Background(), recorder, "client-1")
	if err != nil {
		t.Fatalf("创建 SSE 连接失败: %v", err)
	}

	if client.Id != "client-1" {
		t.Fatalf("客户端 ID 不匹配: %q", client.Id)
	}
	if got := recorder.Header().Get("Content-Type"); got != "text/event-stream" {
		t.Fatalf("Content-Type 不正确: %q", got)
	}
	if !strings.Contains(recorder.Body.String(), "event: connected") {
		t.Fatalf("缺少 connected 事件: %s", recorder.Body.String())
	}
}

func TestSendToClientFormatsMultilineEvent(t *testing.T) {
	service := New()
	recorder := httptest.NewRecorder()
	client, err := service.Create(context.Background(), recorder, "client-2")
	if err != nil {
		t.Fatalf("创建 SSE 连接失败: %v", err)
	}
	recorder.Body.Reset()

	client.SendToClient("message", "第一行\n第二行")

	body := recorder.Body.String()
	if !strings.Contains(body, "event: message\n") {
		t.Fatalf("缺少 message 事件: %s", body)
	}
	if !strings.Contains(body, "data: 第一行\n") || !strings.Contains(body, "data: 第二行\n") {
		t.Fatalf("多行数据格式不正确: %s", body)
	}
}

func TestCreateRejectsWriterWithoutFlusher(t *testing.T) {
	service := New()
	_, err := service.Create(context.Background(), headerOnlyWriter{header: http.Header{}}, "client-3")
	if err == nil {
		t.Fatal("不支持 Flush 的 writer 应返回错误")
	}
}

type headerOnlyWriter struct {
	header http.Header
}

func (w headerOnlyWriter) Header() http.Header {
	return w.header
}

func (w headerOnlyWriter) Write([]byte) (int, error) {
	return 0, nil
}

func (w headerOnlyWriter) WriteHeader(int) {}
