package memory

import (
	"context"
	"regexp"
	"strconv"
	"strings"

	"github.com/cloudwego/eino/schema"
)

// 敏感信息的正则模式列表，用于过滤掉可能包含敏感数据的长期记忆内容。
var sensitivePatterns = []*regexp.Regexp{
	regexp.MustCompile(`(?i)(password|passwd|secret|token|api[_-]?key|authorization)\s*[:=]`),
	regexp.MustCompile(`(?i)bearer\s+[a-z0-9._-]+`),
	regexp.MustCompile(`(?i)sk-[a-z0-9_-]+`),
	regexp.MustCompile(`(?i)(密码|密钥|令牌|凭据)\s*[:：]`),
}

// volatilePatterns 描述不应沉淀为长期记忆的一次性状态或实时指标。
var volatilePatterns = []*regexp.Regexp{
	regexp.MustCompile(`(?i)(当前|现在|刚刚|临时|这次|本次|实时)`),
	regexp.MustCompile(`\d+(\.\d+)?\s*(%|ms|s|秒|分钟|qps|tps)`),
}

// RuleBasedExtractor 是测试和本地工具使用的规则抽取器；生产环境使用 LLMExtractor。
type RuleBasedExtractor struct{}

// NewRuleBasedExtractor 创建规则抽取器。
func NewRuleBasedExtractor() *RuleBasedExtractor {
	return &RuleBasedExtractor{}
}

// Extract 从用户消息中抽取结构化长期记忆候选。
func (e *RuleBasedExtractor) Extract(ctx context.Context, scope MemoryScope, messages []*schema.Message) ([]MemoryCandidate, error) {
	_ = ctx
	if len(messages) == 0 {
		return nil, nil
	}
	candidates := []MemoryCandidate{}
	for _, msg := range messages {
		if msg == nil || msg.Role != schema.User {
			continue
		}
		content := strings.TrimSpace(msg.Content)
		if !looksDurable(content) {
			continue
		}
		candidate := MemoryCandidate{
			Type:        inferMemoryType(content),
			Content:     normalizeMemoryContent(content),
			ScopeLevel:  inferScopeLevel(scope, content),
			Confidence:  inferConfidence(content),
			Reason:      inferReason(content),
			Sensitivity: inferSensitivity(content),
			ShouldStore: true,
		}
		candidates = append(candidates, candidate)
	}
	return candidates, nil
}

// looksDurable 判断内容是否看起来具有长期价值。
func looksDurable(content string) bool {
	content = strings.TrimSpace(content)
	if len([]rune(content)) < 8 {
		return false
	}
	triggers := []string{"记住", "以后", "长期", "固定", "偏好", "约定", "确认", "总是", "默认", "集群", "哨兵", "拓扑", "流程"}
	for _, trigger := range triggers {
		if strings.Contains(content, trigger) {
			return true
		}
	}
	return false
}

// isSafeLongTermContent 判断内容是否安全，主要通过检查是否匹配敏感信息和实时状态模式。
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

func isSafeCandidate(candidate MemoryCandidate) bool {
	if !isSafeLongTermContent(candidate.Content) {
		return false
	}
	if strings.EqualFold(candidate.Sensitivity, "high") {
		return false
	}
	for _, pattern := range volatilePatterns {
		if pattern.MatchString(candidate.Content) {
			return false
		}
	}
	return true
}

func isSafeMessageForExtraction(msg *schema.Message) bool {
	if msg == nil || strings.TrimSpace(msg.Content) == "" {
		return false
	}
	if msg.Role != schema.User {
		return false
	}
	content := strings.TrimSpace(msg.Content)
	if !looksDurable(content) {
		return false
	}
	if !isSafeLongTermContent(content) {
		return false
	}
	for _, pattern := range volatilePatterns {
		if pattern.MatchString(content) {
			return false
		}
	}
	return true
}

// inferMemoryType 根据内容中的关键词推断记忆类型。
func inferMemoryType(content string) string {
	switch {
	case strings.Contains(content, "偏好") || strings.Contains(content, "格式") || strings.Contains(content, "默认用"):
		return "preference"
	case strings.Contains(content, "集群") || strings.Contains(content, "环境") || strings.Contains(content, "哨兵") || strings.Contains(content, "拓扑"):
		return "cluster_knowledge"
	case strings.Contains(content, "事故") || strings.Contains(content, "故障") || strings.Contains(content, "确认"):
		return "incident_note"
	default:
		return "team_note"
	}
}

func inferScopeLevel(scope MemoryScope, content string) ScopeLevel {
	if scope.ClusterID != "" || strings.Contains(content, "集群") || strings.Contains(content, "Redis") || strings.Contains(content, "MySQL") {
		return ScopeLevelCluster
	}
	return ScopeLevelTeam
}

func inferConfidence(content string) float64 {
	if strings.Contains(content, "确认") || strings.Contains(content, "固定") || strings.Contains(content, "默认") || strings.Contains(content, "总是") {
		return 0.88
	}
	if strings.Contains(content, "可能") || strings.Contains(content, "猜") || strings.Contains(content, "怀疑") {
		return 0.5
	}
	return 0.76
}

func inferSensitivity(content string) string {
	for _, pattern := range sensitivePatterns {
		if pattern.MatchString(content) {
			return "high"
		}
	}
	return "low"
}

func inferReason(content string) string {
	if strings.Contains(content, "确认") {
		return "用户表达了已确认的稳定结论"
	}
	if strings.Contains(content, "默认") || strings.Contains(content, "固定") || strings.Contains(content, "总是") {
		return "用户表达了稳定偏好或环境约定"
	}
	return "用户表达的信息包含可复用的长期上下文"
}

func normalizeMemoryContent(content string) string {
	content = strings.TrimSpace(content)
	prefixes := []string{"请记住：", "请记住:", "记住：", "记住:"}
	// 例如：
	// "你好啊，请记住：我喜欢吃苹果。" -> "我喜欢吃苹果。"
	for _, prefix := range prefixes {
		content = strings.TrimPrefix(content, prefix)
	}
	return strings.TrimSpace(content)
}

// FormatLongTermMemories 把长期记忆格式化为可注入 prompt 的文本。
// 例如：
// - [团队:team-1 / preference / 置信度:0.88] 我喜欢吃苹果
// - [集群:cluster-1 / cluster_knowledge] 集群使用 Redis 6.2
func FormatLongTermMemories(items []LongTermMemory) string {
	if len(items) == 0 {
		return "无"
	}
	lines := make([]string, 0, len(items))
	for _, item := range items {
		scope := "团队:" + item.Scope.TeamID
		if item.Scope.ClusterID != "" {
			scope = "集群:" + item.Scope.ClusterID
		}
		confidence := ""
		if item.Confidence > 0 {
			confidence = " / 置信度:" + strings.TrimRight(strings.TrimRight(formatFloat(item.Confidence), "0"), ".")
		}
		lines = append(lines, "- ["+scope+" / "+item.Type+confidence+"] "+item.Content)
	}
	return strings.Join(lines, "\n")
}

func formatFloat(value float64) string {
	return strconv.FormatFloat(value, 'f', 2, 64)
}
