package model

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	dashscope "github.com/cloudwego/eino-ext/components/embedding/dashscope"
	einoopenai "github.com/cloudwego/eino-ext/components/model/openai"
	einoembedding "github.com/cloudwego/eino/components/embedding"
	einomodel "github.com/cloudwego/eino/components/model"
)

var ErrUnavailable = errors.New("model unavailable")

type Message struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type ChatRequest struct {
	Messages    []Message
	Temperature float64
}

type ChatResponse struct {
	Content string
}

type StreamChunk struct {
	Content string
	Err     error
}

type ChatModel interface {
	Complete(ctx context.Context, req ChatRequest) (ChatResponse, error)
	Stream(ctx context.Context, req ChatRequest) (<-chan StreamChunk, error)
}

type Config struct {
	APIKey  string
	BaseURL string
	Model   string
}

type EmbeddingConfig struct {
	APIKey     string
	Model      string
	Dimensions int
}

func NewEinoOpenAIModel(ctx context.Context, cfg Config) (einomodel.ToolCallingChatModel, error) {
	if strings.TrimSpace(cfg.APIKey) == "" || strings.TrimSpace(cfg.BaseURL) == "" || strings.TrimSpace(cfg.Model) == "" {
		return nil, ErrUnavailable
	}
	return einoopenai.NewChatModel(ctx, &einoopenai.ChatModelConfig{
		APIKey:  cfg.APIKey,
		BaseURL: cfg.BaseURL,
		Model:   cfg.Model,
		Timeout: 60 * time.Second,
	})
}

func NewDashScopeEmbedder(ctx context.Context, cfg EmbeddingConfig) (einoembedding.Embedder, error) {
	if strings.TrimSpace(cfg.APIKey) == "" || strings.TrimSpace(cfg.Model) == "" {
		return nil, ErrUnavailable
	}
	dimensions := cfg.Dimensions
	if dimensions <= 0 {
		dimensions = 2048
	}
	return dashscope.NewEmbedder(ctx, &dashscope.EmbeddingConfig{
		APIKey:     cfg.APIKey,
		Model:      cfg.Model,
		Dimensions: &dimensions,
		Timeout:    60 * time.Second,
	})
}

type OpenAICompatible struct {
	cfg  Config
	http *http.Client
}

func NewOpenAICompatible(cfg Config) *OpenAICompatible {
	return &OpenAICompatible{
		cfg:  cfg,
		http: &http.Client{Timeout: 60 * time.Second},
	}
}

func (m *OpenAICompatible) Complete(ctx context.Context, req ChatRequest) (ChatResponse, error) {
	if err := m.available(); err != nil {
		return ChatResponse{}, err
	}
	body := map[string]any{
		"model":       m.cfg.Model,
		"messages":    req.Messages,
		"temperature": req.Temperature,
		"stream":      false,
	}
	raw, err := json.Marshal(body)
	if err != nil {
		return ChatResponse{}, err
	}
	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, strings.TrimRight(m.cfg.BaseURL, "/")+"/chat/completions", bytes.NewReader(raw))
	if err != nil {
		return ChatResponse{}, err
	}
	httpReq.Header.Set("Authorization", "Bearer "+m.cfg.APIKey)
	httpReq.Header.Set("Content-Type", "application/json")
	resp, err := m.http.Do(httpReq)
	if err != nil {
		return ChatResponse{}, err
	}
	defer resp.Body.Close()
	content, err := io.ReadAll(resp.Body)
	if err != nil {
		return ChatResponse{}, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return ChatResponse{}, fmt.Errorf("model returned status %d: %s", resp.StatusCode, strings.TrimSpace(string(content)))
	}
	var parsed struct {
		Choices []struct {
			Message Message `json:"message"`
		} `json:"choices"`
	}
	if err := json.Unmarshal(content, &parsed); err != nil {
		return ChatResponse{}, err
	}
	if len(parsed.Choices) == 0 {
		return ChatResponse{}, fmt.Errorf("model returned no choices")
	}
	return ChatResponse{Content: parsed.Choices[0].Message.Content}, nil
}

func (m *OpenAICompatible) Stream(ctx context.Context, req ChatRequest) (<-chan StreamChunk, error) {
	if err := m.available(); err != nil {
		return nil, err
	}
	out := make(chan StreamChunk)
	go func() {
		defer close(out)
		resp, err := m.Complete(ctx, req)
		if err != nil {
			out <- StreamChunk{Err: err}
			return
		}
		scanner := bufio.NewScanner(strings.NewReader(resp.Content))
		scanner.Split(bufio.ScanRunes)
		for scanner.Scan() {
			select {
			case <-ctx.Done():
				out <- StreamChunk{Err: ctx.Err()}
				return
			case out <- StreamChunk{Content: scanner.Text()}:
			}
		}
	}()
	return out, nil
}

func (m *OpenAICompatible) available() error {
	if strings.TrimSpace(m.cfg.APIKey) == "" || strings.TrimSpace(m.cfg.BaseURL) == "" || strings.TrimSpace(m.cfg.Model) == "" {
		return ErrUnavailable
	}
	return nil
}

type fallbackModel struct {
	text string
}

func Fallback(text string) ChatModel {
	return fallbackModel{text: text}
}

func (m fallbackModel) Complete(ctx context.Context, req ChatRequest) (ChatResponse, error) {
	_ = ctx
	_ = req
	return ChatResponse{Content: m.text}, nil
}

func (m fallbackModel) Stream(ctx context.Context, req ChatRequest) (<-chan StreamChunk, error) {
	_ = req
	out := make(chan StreamChunk)
	go func() {
		defer close(out)
		for _, r := range m.text {
			select {
			case <-ctx.Done():
				out <- StreamChunk{Err: ctx.Err()}
				return
			case out <- StreamChunk{Content: string(r)}:
			}
		}
	}()
	return out, nil
}
