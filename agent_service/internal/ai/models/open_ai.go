package models

import (
	"context"
	"monitor-system/agent_service/internal/config"

	"github.com/cloudwego/eino-ext/components/model/openai"
	"github.com/cloudwego/eino/components/model"
)

// 创建ai model以供agent调用，模型配置从环境变量读取
func OpenAIForThink(ctx context.Context) (cm model.ToolCallingChatModel, err error) {
	modelName, err := config.ConfigString(ctx, "ds_think_chat_model.model", "AGENT_THINK_MODEL", "")
	if err != nil {
		return nil, err
	}
	apiKey, err := config.ConfigString(ctx, "ds_think_chat_model.api_key", "AGENT_THINK_API_KEY", "")
	if err != nil {
		return nil, err
	}
	baseURL, err := config.ConfigString(ctx, "ds_think_chat_model.base_url", "AGENT_THINK_BASE_URL", "")
	if err != nil {
		return nil, err
	}
	config := &openai.ChatModelConfig{
		Model:   modelName,
		APIKey:  apiKey,
		BaseURL: baseURL,
	}
	cm, err = openai.NewChatModel(ctx, config)
	if err != nil {
		return nil, err
	}
	return cm, nil
}

// 创建ai model以供agent调用，模型配置从环境变量读取
func OpenAIForQuick(ctx context.Context) (cm model.ToolCallingChatModel, err error) {
	modelName, err := config.ConfigString(ctx, "ds_quick_chat_model.model", "AGENT_QUICK_MODEL", "")
	if err != nil {
		return nil, err
	}
	apiKey, err := config.ConfigString(ctx, "ds_quick_chat_model.api_key", "AGENT_QUICK_API_KEY", "")
	if err != nil {
		return nil, err
	}
	baseURL, err := config.ConfigString(ctx, "ds_quick_chat_model.base_url", "AGENT_QUICK_BASE_URL", "")
	if err != nil {
		return nil, err
	}
	config := &openai.ChatModelConfig{
		Model:   modelName,
		APIKey:  apiKey,
		BaseURL: baseURL,
	}
	cm, err = openai.NewChatModel(ctx, config)
	if err != nil {
		return nil, err
	}
	return cm, nil
}
