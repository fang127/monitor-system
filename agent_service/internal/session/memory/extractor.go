package memory

import (
	"regexp"
	"strings"
	"time"

	"github.com/cloudwego/eino/schema"
	"github.com/google/uuid"
)

var sensitivePatterns = []*regexp.Regexp{
	regexp.MustCompile(`(?i)(password|passwd|secret|token|api[_-]?key|authorization)\s*[:=]`),
	regexp.MustCompile(`(?i)bearer\s+[a-z0-9._-]+`),
	regexp.MustCompile(`(?i)sk-[a-z0-9_-]+`),
}

func extractCandidates(scope MemoryScope, messages []*schema.Message) []LongTermMemory {
	if len(messages) == 0 {
		return nil
	}
	candidates := []LongTermMemory{}
	for _, msg := range messages {
		if msg == nil || msg.Role != schema.User {
			continue
		}
		content := strings.TrimSpace(msg.Content)
		if !looksDurable(content) {
			continue
		}
		candidates = append(candidates, LongTermMemory{
			ID:         uuid.NewString(),
			Scope:      scope.Normalized(),
			Type:       inferMemoryType(content),
			Content:    content,
			Source:     "chat",
			Confidence: 0.8,
			Status:     LongTermStatusActive,
			CreatedBy:  "auto",
			CreatedAt:  time.Now(),
			UpdatedAt:  time.Now(),
		})
	}
	return candidates
}

func looksDurable(content string) bool {
	content = strings.TrimSpace(content)
	if len([]rune(content)) < 8 {
		return false
	}
	triggers := []string{"记住", "以后", "长期", "固定", "偏好", "约定", "确认", "总是", "默认", "集群"}
	for _, trigger := range triggers {
		if strings.Contains(content, trigger) {
			return true
		}
	}
	return false
}

func isSafeLongTermContent(content string) bool {
	content = strings.TrimSpace(content)
	if content == "" {
		return false
	}
	for _, pattern := range sensitivePatterns {
		if pattern.MatchString(content) {
			return false
		}
	}
	return true
}

func inferMemoryType(content string) string {
	switch {
	case strings.Contains(content, "偏好") || strings.Contains(content, "格式"):
		return "preference"
	case strings.Contains(content, "集群") || strings.Contains(content, "环境"):
		return "cluster_knowledge"
	case strings.Contains(content, "事故") || strings.Contains(content, "故障") || strings.Contains(content, "确认"):
		return "incident_note"
	default:
		return "team_note"
	}
}

// FormatLongTermMemories 把长期记忆格式化为可注入 prompt 的文本。
func FormatLongTermMemories(items []LongTermMemory) string {
	if len(items) == 0 {
		return "无"
	}
	lines := make([]string, 0, len(items))
	for _, item := range items {
		scope := "团队"
		if item.Scope.ClusterID != "" {
			scope = "集群:" + item.Scope.ClusterID
		}
		lines = append(lines, "- ["+scope+" / "+item.Type+"] "+item.Content)
	}
	return strings.Join(lines, "\n")
}
