package tools

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"

	"monitor-system/agent_service/utility/common"

	"github.com/cloudwego/eino/components/tool"
	"github.com/cloudwego/eino/components/tool/utils"
)

// 该文件定义了一组工具函数，用于从监控系统的 API 网关查询集群概览、异常记录、性能数据、趋势数据、通用明细和 MySQL 明细数据。这些工具函数使用 HTTP GET 请求与 API 网关通信，并将响应格式化为统一的输出结构。工具函数支持可选的查询参数，如时间范围、分页和阈值过滤，以满足不同的查询需求。

// gatewayEnvelope 定义了 API 网关响应的通用结构，包含状态码、消息和数据字段。
type gatewayEnvelope struct {
	Code    int             `json:"code"`
	Message string          `json:"message"`
	Data    json.RawMessage `json:"data"`
}

// gatewayToolOutput 定义了工具函数输出的统一结构，包含成功标志、数据来源、返回数据、消息和错误信息。
type gatewayToolOutput struct {
	Success bool        `json:"success"`
	Source  string      `json:"source"`
	Data    interface{} `json:"data,omitempty"`
	Message string      `json:"message,omitempty"`
	Error   string      `json:"error,omitempty"`
}

// ClusterOverviewInput 定义了查询集群概览工具的输入结构，目前没有参数。
type ClusterOverviewInput struct{}

// MonitorAnomaliesInput 定义了查询异常记录工具的输入结构，包含服务器名称、时间范围、分页参数和阈值过滤参数。
type MonitorAnomaliesInput struct {
	ServerName                   string  `json:"server_name" jsonschema:"description=Server name to query. Leave empty to query every server returned by query_monitor_cluster_overview"`
	StartTime                    string  `json:"start_time" jsonschema:"description=Optional start time, RFC3339 or Unix seconds"`
	EndTime                      string  `json:"end_time" jsonschema:"description=Optional end time, RFC3339 or Unix seconds"`
	Page                         int     `json:"page" jsonschema:"description=Optional page number, defaults to 1"`
	PageSize                     int     `json:"page_size" jsonschema:"description=Optional page size, defaults to 50"`
	CPUThreshold                 float64 `json:"cpu_threshold" jsonschema:"description=Optional CPU usage threshold"`
	MemThreshold                 float64 `json:"mem_threshold" jsonschema:"description=Optional memory usage threshold"`
	DiskThreshold                float64 `json:"disk_threshold" jsonschema:"description=Optional disk utilization threshold"`
	ChangeRateThreshold          float64 `json:"change_rate_threshold" jsonschema:"description=Optional metric change-rate threshold"`
	RedisConnectionThreshold     float64 `json:"redis_connection_threshold" jsonschema:"description=Optional Redis connection usage threshold"`
	RedisMemoryThreshold         float64 `json:"redis_memory_threshold" jsonschema:"description=Optional Redis memory usage threshold"`
	RedisHitRateThreshold        float64 `json:"redis_hit_rate_threshold" jsonschema:"description=Optional Redis low hit-rate threshold"`
	RedisReplicationLagThreshold float64 `json:"redis_replication_lag_threshold" jsonschema:"description=Optional Redis replication lag threshold in seconds"`
	RedisSlowlogGrowthThreshold  float64 `json:"redis_slowlog_growth_threshold" jsonschema:"description=Optional Redis slowlog growth threshold"`
}

// MonitorSeriesInput 定义了查询性能数据和趋势数据工具的输入结构，包含服务器名称、时间范围、分页参数和趋势聚合间隔参数。
type MonitorSeriesInput struct {
	ServerName      string `json:"server_name" jsonschema:"description=Server name to query"`
	StartTime       string `json:"start_time" jsonschema:"description=Optional start time, RFC3339 or Unix seconds"`
	EndTime         string `json:"end_time" jsonschema:"description=Optional end time, RFC3339 or Unix seconds"`
	Page            int    `json:"page" jsonschema:"description=Optional page number, defaults to 1. Used by performance and detail queries"`
	PageSize        int    `json:"page_size" jsonschema:"description=Optional page size, defaults to 100. Used by performance and detail queries"`
	IntervalSeconds int    `json:"interval_seconds" jsonschema:"description=Optional trend aggregation interval in seconds. Used by trend queries"`
}

// MonitorDetailInput 定义了查询详细数据工具的输入结构，包含服务器名称、详细数据类型、时间范围和分页参数。
type MonitorDetailInput struct {
	ServerName string `json:"server_name" jsonschema:"description=Server name to query"`
	Kind       string `json:"kind" jsonschema:"description=Detail kind: net, disk, mem, softirq, mysql, or redis"`
	StartTime  string `json:"start_time" jsonschema:"description=Optional start time, RFC3339 or Unix seconds"`
	EndTime    string `json:"end_time" jsonschema:"description=Optional end time, RFC3339 or Unix seconds"`
	Page       int    `json:"page" jsonschema:"description=Optional page number, defaults to 1"`
	PageSize   int    `json:"page_size" jsonschema:"description=Optional page size, defaults to 100"`
}

// MonitorMysqlDetailInput 定义了查询 MySQL 明细数据工具的输入结构。
type MonitorMysqlDetailInput struct {
	ServerName string `json:"server_name" jsonschema:"description=Server name to query"`
	StartTime  string `json:"start_time" jsonschema:"description=Optional start time, RFC3339 or Unix seconds"`
	EndTime    string `json:"end_time" jsonschema:"description=Optional end time, RFC3339 or Unix seconds"`
	Page       int    `json:"page" jsonschema:"description=Optional page number, defaults to 1"`
	PageSize   int    `json:"page_size" jsonschema:"description=Optional page size, defaults to 100"`
}

// MonitorRedisDetailInput 定义了查询 Redis 明细数据工具的输入结构。
type MonitorRedisDetailInput struct {
	ServerName string `json:"server_name" jsonschema:"description=Server name to query"`
	StartTime  string `json:"start_time" jsonschema:"description=Optional start time, RFC3339 or Unix seconds"`
	EndTime    string `json:"end_time" jsonschema:"description=Optional end time, RFC3339 or Unix seconds"`
	Page       int    `json:"page" jsonschema:"description=Optional page number, defaults to 1"`
	PageSize   int    `json:"page_size" jsonschema:"description=Optional page size, defaults to 100"`
}

// NewMonitorClusterOverviewTool 创建了一个工具函数，用于查询监控系统的集群概览。该工具函数发送 GET 请求到 API 网关的 /api/servers/latest 端点，并返回最新的服务器评分、在线/离线状态和集群统计信息。
func NewMonitorClusterOverviewTool() tool.InvokableTool {
	t, err := utils.InferOptionableTool(
		"query_monitor_cluster_overview",
		"Query monitor_system cluster overview from api_gateway. Returns latest server scores, online/offline status, and cluster statistics.",
		func(ctx context.Context, input *ClusterOverviewInput, opts ...tool.Option) (string, error) {
			data, err := apiGatewayGet(ctx, "/api/servers/latest", nil)
			return formatGatewayOutput("/api/servers/latest", data, err)
		},
	)
	if err != nil {
		return errorTool("query_monitor_cluster_overview", err)
	}
	return t
}

func NewMonitorAnomaliesTool() tool.InvokableTool {
	t, err := utils.InferOptionableTool(
		"query_monitor_anomalies",
		"Query monitor_system anomaly records from api_gateway. If server_name is empty, queries all servers from the latest overview.",
		func(ctx context.Context, input *MonitorAnomaliesInput, opts ...tool.Option) (string, error) {
			if input == nil {
				input = &MonitorAnomaliesInput{}
			}
			params := anomalyQuery(input)
			if strings.TrimSpace(input.ServerName) != "" {
				path := fmt.Sprintf("/api/servers/%s/anomalies", url.PathEscape(input.ServerName))
				data, err := apiGatewayGet(ctx, path, params)
				return formatGatewayOutput(path, data, err)
			}
			data, err := queryAllServerAnomalies(ctx, params)
			return formatGatewayOutput("/api/servers/*/anomalies", data, err)
		},
	)
	if err != nil {
		return errorTool("query_monitor_anomalies", err)
	}
	return t
}

func NewMonitorPerformanceTool() tool.InvokableTool {
	t, err := utils.InferOptionableTool(
		"query_monitor_performance",
		"Query monitor_system historical performance records for one server from api_gateway.",
		func(ctx context.Context, input *MonitorSeriesInput, opts ...tool.Option) (string, error) {
			if input == nil || strings.TrimSpace(input.ServerName) == "" {
				return formatGatewayOutput("/api/servers/:server/performance", nil, fmt.Errorf("server_name is required"))
			}
			path := fmt.Sprintf("/api/servers/%s/performance", url.PathEscape(input.ServerName))
			data, err := apiGatewayGet(ctx, path, seriesQuery(input, true, false))
			return formatGatewayOutput(path, data, err)
		},
	)
	if err != nil {
		return errorTool("query_monitor_performance", err)
	}
	return t
}

func NewMonitorTrendTool() tool.InvokableTool {
	t, err := utils.InferOptionableTool(
		"query_monitor_trend",
		"Query monitor_system metric trend records for one server from api_gateway.",
		func(ctx context.Context, input *MonitorSeriesInput, opts ...tool.Option) (string, error) {
			if input == nil || strings.TrimSpace(input.ServerName) == "" {
				return formatGatewayOutput("/api/servers/:server/trend", nil, fmt.Errorf("server_name is required"))
			}
			path := fmt.Sprintf("/api/servers/%s/trend", url.PathEscape(input.ServerName))
			data, err := apiGatewayGet(ctx, path, seriesQuery(input, false, true))
			return formatGatewayOutput(path, data, err)
		},
	)
	if err != nil {
		return errorTool("query_monitor_trend", err)
	}
	return t
}

func NewMonitorDetailTool() tool.InvokableTool {
	t, err := utils.InferOptionableTool(
		"query_monitor_detail",
		"Query monitor_system detail records for one server from api_gateway. kind must be net, disk, mem, softirq, mysql, or redis. Use kind=redis for Redis availability, connection pressure, memory pressure, commands/s, hit rate, replication lag, and slowlog growth.",
		func(ctx context.Context, input *MonitorDetailInput, opts ...tool.Option) (string, error) {
			if input == nil || strings.TrimSpace(input.ServerName) == "" {
				return formatGatewayOutput("/api/servers/:server/:kind-detail", nil, fmt.Errorf("server_name is required"))
			}
			endpoint, err := detailEndpoint(input.Kind)
			if err != nil {
				return formatGatewayOutput("/api/servers/:server/:kind-detail", nil, err)
			}
			path := fmt.Sprintf("/api/servers/%s/%s", url.PathEscape(input.ServerName), endpoint)
			data, err := apiGatewayGet(ctx, path, detailQuery(input))
			return formatGatewayOutput(path, data, err)
		},
	)
	if err != nil {
		return errorTool("query_monitor_detail", err)
	}
	return t
}

func NewMonitorRedisDetailTool() tool.InvokableTool {
	t, err := utils.InferOptionableTool(
		"query_monitor_redis_detail",
		"Query monitor_system Redis detail records for one server from api_gateway. Returns Redis availability, connection pressure, memory pressure, commands/s, hit rate, evictions, rejected connections, replication lag, and slowlog growth. This tool reads monitoring facts only and never connects directly to Redis.",
		func(ctx context.Context, input *MonitorRedisDetailInput, opts ...tool.Option) (string, error) {
			if input == nil || strings.TrimSpace(input.ServerName) == "" {
				return formatGatewayOutput("/api/servers/:server/redis-detail", nil, fmt.Errorf("server_name is required"))
			}
			path := fmt.Sprintf("/api/servers/%s/redis-detail", url.PathEscape(input.ServerName))
			data, err := apiGatewayGet(ctx, path, redisDetailQuery(input))
			return formatGatewayOutput(path, data, err)
		},
	)
	if err != nil {
		return errorTool("query_monitor_redis_detail", err)
	}
	return t
}

func NewMonitorMysqlDetailTool() tool.InvokableTool {
	t, err := utils.InferOptionableTool(
		"query_monitor_mysql_detail",
		"Query monitor_system MySQL detail records for one server from api_gateway. Returns MySQL availability, connection pressure, QPS/TPS, slow query rate, InnoDB row lock waits, buffer pool hit rate, and replication lag. This tool reads monitoring facts only and never connects directly to MySQL.",
		func(ctx context.Context, input *MonitorMysqlDetailInput, opts ...tool.Option) (string, error) {
			if input == nil || strings.TrimSpace(input.ServerName) == "" {
				return formatGatewayOutput("/api/servers/:server/mysql-detail", nil, fmt.Errorf("server_name is required"))
			}
			path := fmt.Sprintf("/api/servers/%s/mysql-detail", url.PathEscape(input.ServerName))
			data, err := apiGatewayGet(ctx, path, mysqlDetailQuery(input))
			return formatGatewayOutput(path, data, err)
		},
	)
	if err != nil {
		return errorTool("query_monitor_mysql_detail", err)
	}
	return t
}

func apiGatewayGet(ctx context.Context, path string, query url.Values) (json.RawMessage, error) {
	baseURL, err := common.ConfigString(ctx, "api_gateway_base_url", "API_GATEWAY_BASE_URL", "http://127.0.0.1:8080")
	if err != nil {
		return nil, err
	}
	endpoint := strings.TrimRight(baseURL, "/") + path
	if len(query) > 0 {
		endpoint += "?" + query.Encode()
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return nil, err
	}
	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("api_gateway returned status %d: %s", resp.StatusCode, string(body))
	}
	var envelope gatewayEnvelope
	if err := json.Unmarshal(body, &envelope); err == nil && envelope.Message != "" {
		if envelope.Code != 0 {
			return nil, fmt.Errorf("api_gateway error %d: %s", envelope.Code, envelope.Message)
		}
		return envelope.Data, nil
	}
	return json.RawMessage(body), nil
}

func queryAllServerAnomalies(ctx context.Context, params url.Values) (json.RawMessage, error) {
	overview, err := apiGatewayGet(ctx, "/api/servers/latest", nil)
	if err != nil {
		return nil, err
	}
	var latest struct {
		Servers []struct {
			ServerName string `json:"server_name"`
		} `json:"servers"`
	}
	if err := json.Unmarshal(overview, &latest); err != nil {
		return nil, err
	}
	results := make([]map[string]interface{}, 0, len(latest.Servers))
	for _, server := range latest.Servers {
		if server.ServerName == "" {
			continue
		}
		path := fmt.Sprintf("/api/servers/%s/anomalies", url.PathEscape(server.ServerName))
		data, err := apiGatewayGet(ctx, path, cloneValues(params))
		result := map[string]interface{}{"server_name": server.ServerName}
		if err != nil {
			result["success"] = false
			result["error"] = err.Error()
		} else {
			var payload interface{}
			if json.Unmarshal(data, &payload) == nil {
				result["success"] = true
				result["data"] = payload
			} else {
				result["success"] = true
				result["data"] = string(data)
			}
		}
		results = append(results, result)
	}
	out, err := json.Marshal(map[string]interface{}{
		"servers": results,
	})
	if err != nil {
		return nil, err
	}
	return out, nil
}

func anomalyQuery(input *MonitorAnomaliesInput) url.Values {
	values := url.Values{}
	addTimeRange(values, input.StartTime, input.EndTime)
	addInt(values, "page", input.Page, 1)
	addInt(values, "page_size", input.PageSize, 50)
	addFloat(values, "cpu_threshold", input.CPUThreshold)
	addFloat(values, "mem_threshold", input.MemThreshold)
	addFloat(values, "disk_threshold", input.DiskThreshold)
	addFloat(values, "change_rate_threshold", input.ChangeRateThreshold)
	addFloat(values, "redis_connection_threshold", input.RedisConnectionThreshold)
	addFloat(values, "redis_memory_threshold", input.RedisMemoryThreshold)
	addFloat(values, "redis_hit_rate_threshold", input.RedisHitRateThreshold)
	addFloat(values, "redis_replication_lag_threshold", input.RedisReplicationLagThreshold)
	addFloat(values, "redis_slowlog_growth_threshold", input.RedisSlowlogGrowthThreshold)
	return values
}

func seriesQuery(input *MonitorSeriesInput, pagination bool, interval bool) url.Values {
	values := url.Values{}
	addTimeRange(values, input.StartTime, input.EndTime)
	if pagination {
		addInt(values, "page", input.Page, 1)
		addInt(values, "page_size", input.PageSize, 100)
	}
	if interval {
		addInt(values, "interval_seconds", input.IntervalSeconds, 0)
	}
	return values
}

func detailQuery(input *MonitorDetailInput) url.Values {
	values := url.Values{}
	addTimeRange(values, input.StartTime, input.EndTime)
	addInt(values, "page", input.Page, 1)
	addInt(values, "page_size", input.PageSize, 100)
	return values
}

func mysqlDetailQuery(input *MonitorMysqlDetailInput) url.Values {
	values := url.Values{}
	addTimeRange(values, input.StartTime, input.EndTime)
	addInt(values, "page", input.Page, 1)
	addInt(values, "page_size", input.PageSize, 100)
	return values
}

func redisDetailQuery(input *MonitorRedisDetailInput) url.Values {
	values := url.Values{}
	addTimeRange(values, input.StartTime, input.EndTime)
	addInt(values, "page", input.Page, 1)
	addInt(values, "page_size", input.PageSize, 100)
	return values
}

func addTimeRange(values url.Values, start string, end string) {
	if strings.TrimSpace(start) != "" {
		values.Set("start_time", strings.TrimSpace(start))
	}
	if strings.TrimSpace(end) != "" {
		values.Set("end_time", strings.TrimSpace(end))
	}
}

func addInt(values url.Values, key string, value int, fallback int) {
	if value > 0 {
		values.Set(key, fmt.Sprintf("%d", value))
		return
	}
	if fallback > 0 {
		values.Set(key, fmt.Sprintf("%d", fallback))
	}
}

func addFloat(values url.Values, key string, value float64) {
	if value > 0 {
		values.Set(key, fmt.Sprintf("%g", value))
	}
}

func detailEndpoint(kind string) (string, error) {
	switch strings.ToLower(strings.TrimSpace(kind)) {
	case "net", "network":
		return "net-detail", nil
	case "disk":
		return "disk-detail", nil
	case "mem", "memory":
		return "mem-detail", nil
	case "softirq", "soft_irq":
		return "softirq-detail", nil
	case "mysql", "mysql_detail", "db":
		return "mysql-detail", nil
	case "redis", "redis_detail", "cache":
		return "redis-detail", nil
	default:
		return "", fmt.Errorf("unsupported detail kind %q, expected net, disk, mem, softirq, mysql, or redis", kind)
	}
}

func formatGatewayOutput(source string, data json.RawMessage, err error) (string, error) {
	if err != nil {
		out, _ := json.Marshal(gatewayToolOutput{
			Success: false,
			Source:  source,
			Error:   err.Error(),
		})
		return string(out), err
	}
	var payload interface{}
	if len(data) > 0 && json.Unmarshal(data, &payload) != nil {
		payload = string(data)
	}
	out, err := json.Marshal(gatewayToolOutput{
		Success: true,
		Source:  source,
		Data:    payload,
		Message: "api_gateway query completed",
	})
	if err != nil {
		return "", err
	}
	return string(out), nil
}

func cloneValues(values url.Values) url.Values {
	cloned := url.Values{}
	for key, list := range values {
		for _, value := range list {
			cloned.Add(key, value)
		}
	}
	return cloned
}

func errorTool(name string, buildErr error) tool.InvokableTool {
	t, _ := utils.InferOptionableTool(
		name,
		"Tool failed to initialize; returns the initialization error.",
		func(ctx context.Context, input *struct{}, opts ...tool.Option) (string, error) {
			return formatGatewayOutput(name, nil, buildErr)
		},
	)
	return t
}
