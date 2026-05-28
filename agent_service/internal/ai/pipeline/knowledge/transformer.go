package knowledgepipeline

import (
	"context"

	"github.com/cloudwego/eino-ext/components/document/transformer/splitter/markdown"
	"github.com/cloudwego/eino/components/document"
	"github.com/google/uuid"
)

// newDocumentTransformer创建一个新的文档转换器，用于将文档按照Markdown标题进行分割，并为每个分割后的文档生成一个唯一的ID。
func newDocumentTransformer(ctx context.Context) (tfr document.Transformer, err error) {
	config := &markdown.HeaderConfig{
		// Headers指定了要根据哪些Markdown标题进行分割，这里使用了一级标题（#）作为分割点，并将其映射为文档的标题字段。
		Headers: map[string]string{
			"#": "title",
		},
		// TrimHeaders指定是否在分割后的文档中保留Markdown标题，这里设置为false表示不保留标题。
		TrimHeaders: false,
		// IDGenerator是一个函数，用于为每个分割后的文档生成一个唯一的ID。这里使用了uuid库生成一个新的UUID字符串作为ID。
		IDGenerator: func(ctx context.Context, originalID string, splitIndex int) string {
			return uuid.New().String()
		},
	}
	tfr, err = markdown.NewHeaderSplitter(ctx, config)
	if err != nil {
		return nil, err
	}
	return tfr, nil
}
