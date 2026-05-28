package callback

import (
	"context"
	"encoding/json"
	"fmt"

	"github.com/cloudwego/eino/callbacks"
)

// 该文件实现Eino callback 日志回调函数，主要用于打印Eino组件的输入输出信息，方便调试和监控。

// LogCallbackConfig 是日志回调函数的配置结构体，包含以下字段：
type LogCallbackConfig struct {
	Detail bool // 是否打印详细信息，默认为true
	Debug  bool // 是否以调试模式打印输入信息，默认为false
}

// LogCallback 返回一个Eino回调函数，用于打印Eino组件的输入输出信息。该函数接受一个LogCallbackConfig参数，用于配置日志输出的详细程度和调试模式。
func LogCallback(config *LogCallbackConfig) callbacks.Handler {
	if config == nil {
		config = &LogCallbackConfig{
			Detail: true,
		}
	}

	builder := callbacks.NewHandlerBuilder()
	builder.OnStartFn(func(ctx context.Context, info *callbacks.RunInfo, input callbacks.CallbackInput) context.Context {
		fmt.Printf("[view start]:[%s:%s:%s]\n", info.Component, info.Type, info.Name)
		if config.Detail {
			var b []byte
			// 如果Debug模式开启，则以格式化的方式打印输入信息，否则以紧凑的方式打印输入信息。
			if config.Debug {
				b, _ = json.MarshalIndent(input, "", "  ")
			} else {
				b, _ = json.Marshal(input)
			}
			fmt.Printf("%s\n", string(b))
		}
		return ctx
	})
	builder.OnEndFn(func(ctx context.Context, info *callbacks.RunInfo, output callbacks.CallbackOutput) context.Context {
		fmt.Printf("[view end]:[%s:%s:%s]\n", info.Component, info.Type, info.Name)
		return ctx
	})
	return builder.Build()
}
