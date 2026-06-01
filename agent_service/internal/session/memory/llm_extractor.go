package memory

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"github.com/cloudwego/eino/schema"
)

// StructuredExtractorModel 是结构化记忆抽取所需的最小 LLM 接口。
type StructuredExtractorModel interface {
	Generate(ctx context.Context, prompt string) (string, error)
}

// LLMExtractor 使用快速模型抽取结构化长期记忆候选。
type LLMExtractor struct {
	model StructuredExtractorModel
}

// NewLLMExtractor 创建 LLM 结构化抽取器。
func NewLLMExtractor(model StructuredExtractorModel) *LLMExtractor {
	return &LLMExtractor{model: model}
}

type llmExtractResponse struct {
	Candidates []llmCandidate `json:"candidates"` // 候选项列表
}

type llmCandidate struct {
	Type        string  `json:"type"`
	Content     string  `json:"content"`
	ScopeLevel  string  `json:"scope_level"`
	Confidence  float64 `json:"confidence"`
	Reason      string  `json:"reason"`
	ExpiresAt   string  `json:"expires_at"`
	Sensitivity string  `json:"sensitivity"`
	ShouldStore bool    `json:"should_store"`
}

// Extract 调用 LLM 输出结构化 JSON，并转换为治理候选。
// 触发时机：每轮对话结束后，或用户主动请求记忆总结时。
func (e *LLMExtractor) Extract(ctx context.Context, scope MemoryScope, messages []*schema.Message) ([]MemoryCandidate, error) {
	if e == nil || e.model == nil {
		return nil, fmt.Errorf("LLM 记忆抽取器未配置")
	}
	prompt := buildExtractorPrompt(scope, messages)
	raw, err := e.model.Generate(ctx, prompt)
	if err != nil {
		return nil, err
	}
	var parsed llmExtractResponse
	if err := json.Unmarshal([]byte(extractJSONObject(raw)), &parsed); err != nil {
		return nil, err
	}
	candidates := make([]MemoryCandidate, 0, len(parsed.Candidates))
	for _, item := range parsed.Candidates {
		candidate := MemoryCandidate{
			Type:        strings.TrimSpace(item.Type),
			Content:     strings.TrimSpace(item.Content),
			ScopeLevel:  ScopeLevel(strings.TrimSpace(item.ScopeLevel)),
			Confidence:  item.Confidence,
			Reason:      strings.TrimSpace(item.Reason),
			Sensitivity: strings.TrimSpace(item.Sensitivity),
			ShouldStore: item.ShouldStore,
		}
		if strings.TrimSpace(item.ExpiresAt) != "" {
			if expiresAt, err := time.Parse(time.RFC3339, strings.TrimSpace(item.ExpiresAt)); err == nil {
				candidate.ExpiresAt = &expiresAt
			}
		}
		candidates = append(candidates, candidate)
	}
	return candidates, nil
}

// buildExtractorPrompt 构建 LLM 提示，指导其从对话中抽取结构化记忆候选。
func buildExtractorPrompt(scope MemoryScope, messages []*schema.Message) string {
	parts := []string{
		"你是 agent_service 的长期记忆抽取器。只输出 JSON，不要输出解释。",
		"只保存稳定、可复用、非敏感的信息：偏好、环境约定、集群拓扑、已确认事故结论、团队流程。",
		"禁止保存：密码、token、一次性排障中间状态、未确认猜测、实时指标值。",
		"JSON schema: {\"candidates\":[{\"type\":\"preference|cluster_knowledge|incident_note|team_note\",\"content\":\"...\",\"scope_level\":\"team|cluster\",\"confidence\":0.0,\"reason\":\"...\",\"expires_at\":null,\"sensitivity\":\"low|medium|high\",\"should_store\":true}]}。",
		fmt.Sprintf("当前作用域: tenant=%s team=%s cluster=%s user=%s session=%s", scope.TenantID, scope.TeamID, scope.ClusterID, scope.UserID, scope.SessionID),
		"对话内容:",
	}
	for _, msg := range messages {
		if msg == nil || strings.TrimSpace(msg.Content) == "" {
			continue
		}
		role := "assistant"
		if msg.Role == schema.User {
			role = "user"
		}
		parts = append(parts, role+": "+msg.Content)
	}
	return strings.Join(parts, "\n")
}

// extractJSONObject 从 LLM 输出中提取 JSON 对象，容错处理多余文本。
func extractJSONObject(raw string) string {
	raw = strings.TrimSpace(raw)
	start := strings.Index(raw, "{")
	end := strings.LastIndex(raw, "}")
	if start >= 0 && end >= start {
		return raw[start : end+1]
	}
	return raw
}
