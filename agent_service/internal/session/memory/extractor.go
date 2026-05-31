package memory

import (
	"regexp"
	"strings"
	"time"

	"github.com/cloudwego/eino/schema"
	"github.com/google/uuid"
)

// 敏感信息的正则模式列表，用于过滤掉可能包含敏感数据的长期记忆内容。
var sensitivePatterns = []*regexp.Regexp{
	regexp.MustCompile(`(?i)(password|passwd|secret|token|api[_-]?key|authorization)\s*[:=]`),
	regexp.MustCompile(`(?i)bearer\s+[a-z0-9._-]+`),
	regexp.MustCompile(`(?i)sk-[a-z0-9_-]+`),
}

// ExtractLongTermMemories 从对话消息中提取可能的长期记忆候选项。
func extractCandidates(scope MemoryScope, messages []*schema.Message) []LongTermMemory {
	if len(messages) == 0 {
		return nil
	}
	candidates := []LongTermMemory{}
	for _, msg := range messages {
		// 只考虑用户输入的消息，且内容看起来具有长期价值。
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

// looksDurable 判断内容是否看起来具有长期价值，主要通过检查是否包含一些触发词。
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

// isSafeLongTermContent 判断内容是否安全，主要通过检查是否匹配敏感信息的正则模式。
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

// inferMemoryType 根据内容中的关键词推断记忆的类型，主要分为偏好设置、集群知识、事故记录和团队笔记。
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
