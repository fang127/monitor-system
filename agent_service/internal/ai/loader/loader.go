package loader

import (
	"context"

	"github.com/cloudwego/eino-ext/components/document/loader/file"
	"github.com/cloudwego/eino/components/document"
)

// NewFileLoader创建一个新的文件加载器实例
func NewFileLoader(ctx context.Context) (ldr document.Loader, err error) {
	config := &file.FileLoaderConfig{}
	ldr, err = file.NewFileLoader(ctx, config)
	if err != nil {
		return nil, err
	}
	return ldr, nil
}
