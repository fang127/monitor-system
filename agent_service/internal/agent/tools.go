package agent

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/cloudwego/eino/components/tool"
	"github.com/cloudwego/eino/components/tool/utils"

	"monitor-system/agent_service/internal/gateway"
	"monitor-system/agent_service/internal/knowledge"
)

type toolOutput struct {
	Success bool   `json:"success"`
	Source  string `json:"source"`
	Data    any    `json:"data,omitempty"`
	Message string `json:"message,omitempty"`
	Error   string `json:"error,omitempty"`
}

type emptyInput struct{}

type anomaliesInput struct {
	ServerName string `json:"server_name" jsonschema:"description=Server name to query"`
	StartTime  string `json:"start_time" jsonschema:"description=Optional RFC3339 or Unix seconds start time"`
	EndTime    string `json:"end_time" jsonschema:"description=Optional RFC3339 or Unix seconds end time"`
	Page       int    `json:"page" jsonschema:"description=Optional page number"`
	PageSize   int    `json:"page_size" jsonschema:"description=Optional page size"`
}

type seriesInput struct {
	ServerName      string `json:"server_name" jsonschema:"description=Server name to query"`
	StartTime       string `json:"start_time" jsonschema:"description=Optional RFC3339 or Unix seconds start time"`
	EndTime         string `json:"end_time" jsonschema:"description=Optional RFC3339 or Unix seconds end time"`
	Page            int    `json:"page" jsonschema:"description=Optional page number"`
	PageSize        int    `json:"page_size" jsonschema:"description=Optional page size"`
	IntervalSeconds int    `json:"interval_seconds" jsonschema:"description=Optional trend interval seconds"`
}

type detailInput struct {
	ServerName string `json:"server_name" jsonschema:"description=Server name to query"`
	Kind       string `json:"kind" jsonschema:"description=Detail kind: net, disk, mem, softirq, mysql, or redis"`
	StartTime  string `json:"start_time" jsonschema:"description=Optional RFC3339 or Unix seconds start time"`
	EndTime    string `json:"end_time" jsonschema:"description=Optional RFC3339 or Unix seconds end time"`
	Page       int    `json:"page" jsonschema:"description=Optional page number"`
	PageSize   int    `json:"page_size" jsonschema:"description=Optional page size"`
}

type docsInput struct {
	Query string `json:"query" jsonschema:"description=Question or keyword to search in uploaded internal docs"`
}

func BuildMonitorTools(g Gateway, store knowledge.Store) []tool.BaseTool {
	tools := []tool.BaseTool{
		mustTool(utils.InferOptionableTool("query_monitor_cluster_overview", "Query monitor_system cluster overview from api_gateway.", func(ctx context.Context, input *emptyInput, opts ...tool.Option) (string, error) {
			if g == nil {
				return formatToolOutput("/api/servers/latest", nil, fmt.Errorf("api_gateway client is not configured"))
			}
			_, raw, err := g.LatestTyped(ctx)
			return formatToolOutput("/api/servers/latest", raw, err)
		})),
		mustTool(utils.InferOptionableTool("query_monitor_anomalies", "Query monitor_system anomaly records for one server from api_gateway.", func(ctx context.Context, input *anomaliesInput, opts ...tool.Option) (string, error) {
			if g == nil {
				return formatToolOutput("/api/servers/:server/anomalies", nil, fmt.Errorf("api_gateway client is not configured"))
			}
			if input == nil || input.ServerName == "" {
				return formatToolOutput("/api/servers/:server/anomalies", nil, fmt.Errorf("server_name is required"))
			}
			raw, err := g.Anomalies(ctx, input.ServerName, toQuery(input.StartTime, input.EndTime, input.Page, input.PageSize, 0))
			return formatToolOutput("/api/servers/"+input.ServerName+"/anomalies", raw, err)
		})),
		mustTool(utils.InferOptionableTool("query_monitor_performance", "Query monitor_system historical performance records for one server from api_gateway.", func(ctx context.Context, input *seriesInput, opts ...tool.Option) (string, error) {
			if g == nil {
				return formatToolOutput("/api/servers/:server/performance", nil, fmt.Errorf("api_gateway client is not configured"))
			}
			if input == nil || input.ServerName == "" {
				return formatToolOutput("/api/servers/:server/performance", nil, fmt.Errorf("server_name is required"))
			}
			raw, err := g.Performance(ctx, input.ServerName, toQuery(input.StartTime, input.EndTime, input.Page, input.PageSize, 0))
			return formatToolOutput("/api/servers/"+input.ServerName+"/performance", raw, err)
		})),
		mustTool(utils.InferOptionableTool("query_monitor_trend", "Query monitor_system trend records for one server from api_gateway.", func(ctx context.Context, input *seriesInput, opts ...tool.Option) (string, error) {
			if g == nil {
				return formatToolOutput("/api/servers/:server/trend", nil, fmt.Errorf("api_gateway client is not configured"))
			}
			if input == nil || input.ServerName == "" {
				return formatToolOutput("/api/servers/:server/trend", nil, fmt.Errorf("server_name is required"))
			}
			raw, err := g.Trend(ctx, input.ServerName, toQuery(input.StartTime, input.EndTime, 0, 0, input.IntervalSeconds))
			return formatToolOutput("/api/servers/"+input.ServerName+"/trend", raw, err)
		})),
		mustTool(utils.InferOptionableTool("query_monitor_detail", "Query monitor_system detail records for one server from api_gateway.", func(ctx context.Context, input *detailInput, opts ...tool.Option) (string, error) {
			if g == nil {
				return formatToolOutput("/api/servers/:server/:kind-detail", nil, fmt.Errorf("api_gateway client is not configured"))
			}
			if input == nil || input.ServerName == "" {
				return formatToolOutput("/api/servers/:server/:kind-detail", nil, fmt.Errorf("server_name is required"))
			}
			raw, err := g.Detail(ctx, input.ServerName, input.Kind, toQuery(input.StartTime, input.EndTime, input.Page, input.PageSize, 0))
			return formatToolOutput("/api/servers/"+input.ServerName+"/"+input.Kind+"-detail", raw, err)
		})),
		mustTool(utils.InferOptionableTool("query_monitor_mysql_detail", "Query monitor_system MySQL detail records for one server from api_gateway.", func(ctx context.Context, input *detailInput, opts ...tool.Option) (string, error) {
			if g == nil {
				return formatToolOutput("/api/servers/:server/mysql-detail", nil, fmt.Errorf("api_gateway client is not configured"))
			}
			if input == nil || input.ServerName == "" {
				return formatToolOutput("/api/servers/:server/mysql-detail", nil, fmt.Errorf("server_name is required"))
			}
			raw, err := g.Detail(ctx, input.ServerName, "mysql", toQuery(input.StartTime, input.EndTime, input.Page, input.PageSize, 0))
			return formatToolOutput("/api/servers/"+input.ServerName+"/mysql-detail", raw, err)
		})),
		mustTool(utils.InferOptionableTool("query_monitor_redis_detail", "Query monitor_system Redis detail records for one server from api_gateway.", func(ctx context.Context, input *detailInput, opts ...tool.Option) (string, error) {
			if g == nil {
				return formatToolOutput("/api/servers/:server/redis-detail", nil, fmt.Errorf("api_gateway client is not configured"))
			}
			if input == nil || input.ServerName == "" {
				return formatToolOutput("/api/servers/:server/redis-detail", nil, fmt.Errorf("server_name is required"))
			}
			raw, err := g.Detail(ctx, input.ServerName, "redis", toQuery(input.StartTime, input.EndTime, input.Page, input.PageSize, 0))
			return formatToolOutput("/api/servers/"+input.ServerName+"/redis-detail", raw, err)
		})),
		mustTool(utils.InferOptionableTool("query_internal_docs", "Search uploaded internal operations documents.", func(ctx context.Context, input *docsInput, opts ...tool.Option) (string, error) {
			if input == nil {
				input = &docsInput{}
			}
			if store == nil {
				return formatToolOutput("internal_docs", nil, fmt.Errorf("knowledge store is not configured"))
			}
			docs, err := store.Search(ctx, input.Query, 3)
			return formatToolOutput("internal_docs", docs, err)
		})),
		mustTool(utils.InferOptionableTool("get_current_time", "Get current server time.", func(ctx context.Context, input *emptyInput, opts ...tool.Option) (string, error) {
			return formatToolOutput("time", map[string]string{"now": time.Now().Format(time.RFC3339)}, nil)
		})),
	}
	return tools
}

func mustTool(t tool.InvokableTool, err error) tool.InvokableTool {
	if err != nil {
		fallback, _ := utils.InferOptionableTool("tool_init_error", "Tool initialization error.", func(ctx context.Context, input *emptyInput, opts ...tool.Option) (string, error) {
			return formatToolOutput("tool_init_error", nil, err)
		})
		return fallback
	}
	return t
}

func toQuery(start, end string, page int, pageSize int, interval int) gateway.Query {
	if page <= 0 {
		page = 1
	}
	if pageSize <= 0 {
		pageSize = 50
	}
	return gateway.Query{StartTime: start, EndTime: end, Page: page, PageSize: pageSize, IntervalSeconds: interval}
}

func formatToolOutput(source string, data any, err error) (string, error) {
	out := toolOutput{Success: err == nil, Source: source, Data: data, Message: "ok"}
	if err != nil {
		out.Message = ""
		out.Error = err.Error()
	}
	raw, marshalErr := json.Marshal(out)
	if marshalErr != nil {
		return "", marshalErr
	}
	if err != nil {
		return string(raw), err
	}
	return string(raw), nil
}
