package memory

import (
	"context"

	"github.com/cloudwego/eino/components/model"
	"github.com/cloudwego/eino/schema"
)

// EinoExtractorModel 把 Eino quick chat model 适配为结构化抽取器需要的文本模型。
type ExtractorModel struct {
	model model.BaseChatModel
}

// NewEinoExtractorModel 创建 Eino 模型适配器。
func NewEinoExtractorModel(model model.BaseChatModel) *ExtractorModel {
	return &ExtractorModel{model: model}
}

// Generate 调用 quick model 生成结构化 JSON。
func (m *ExtractorModel) Generate(ctx context.Context, prompt string) (string, error) {
	out, err := m.model.Generate(ctx, []*schema.Message{schema.UserMessage(prompt)})
	if err != nil {
		return "", err
	}
	if out == nil {
		return "", nil
	}
	return out.Content, nil
}
