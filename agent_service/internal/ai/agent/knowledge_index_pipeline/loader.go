package knowledge_index_pipeline

import (
	"context"

	"github.com/cloudwego/eino-ext/components/document/loader/file"
	"github.com/cloudwego/eino/components/document"
)

// newLoader创建一个新的文档加载器实例。根据需要，可以在此函数中修改组件的配置。
func newLoader(ctx context.Context) (ldr document.Loader, err error) {
	// 待办：在这里修改组件配置。
	config := &file.FileLoaderConfig{}
	ldr, err = file.NewFileLoader(ctx, config)
	if err != nil {
		return nil, err
	}
	return ldr, nil
}
