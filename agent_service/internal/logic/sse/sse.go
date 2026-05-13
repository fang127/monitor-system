package sse

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net/http"
	"strings"
	"sync"
	"time"
)

// Client 表示SSE客户端连接
type Client struct {
	Id          string
	Writer      http.ResponseWriter
	flusher     http.Flusher
	messageChan chan string
	mu          sync.Mutex
}

// Service SSE服务
type Service struct {
	clients sync.Map // 存储所有客户端连接
}

// New 创建SSE服务实例
func New() *Service {
	return &Service{}
}

// Create 创建SSE连接
func (s *Service) Create(ctx context.Context, writer http.ResponseWriter, clientId string) (*Client, error) {
	_ = ctx
	flusher, ok := writer.(http.Flusher)
	if !ok {
		return nil, fmt.Errorf("streaming is not supported")
	}
	// 设置SSE必要的HTTP头
	header := writer.Header()
	header.Set("Content-Type", "text/event-stream")
	header.Set("Cache-Control", "no-cache")
	header.Set("Connection", "keep-alive")
	header.Set("Access-Control-Allow-Origin", "*")
	// 创建新客户端
	if strings.TrimSpace(clientId) == "" {
		clientId = newClientID()
	}
	client := &Client{
		Id:          clientId,
		Writer:      writer,
		flusher:     flusher,
		messageChan: make(chan string, 100),
	}
	s.clients.Store(clientId, client)
	// 发送连接成功消息
	client.SendToClient("connected", fmt.Sprintf(`{"status":"connected","client_id":"%s"}`, clientId))
	return client, nil
}

// SendToClient 向指定客户端发送消息
func (c *Client) SendToClient(eventType, data string) bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	_, _ = fmt.Fprintf(c.Writer, "id: %d\n", time.Now().UnixNano())
	_, _ = fmt.Fprintf(c.Writer, "event: %s\n", eventType)
	for _, line := range strings.Split(data, "\n") {
		_, _ = fmt.Fprintf(c.Writer, "data: %s\n", line)
	}
	_, _ = fmt.Fprint(c.Writer, "\n")
	// 尝试发送消息，如果缓冲区满则跳过
	c.flusher.Flush()
	return true
}

func newClientID() string {
	var b [16]byte
	if _, err := rand.Read(b[:]); err == nil {
		return hex.EncodeToString(b[:])
	}
	return fmt.Sprintf("%d", time.Now().UnixNano())
}
