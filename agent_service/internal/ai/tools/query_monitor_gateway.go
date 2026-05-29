package tools

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	authctx "monitor-system/agent_service/internal/auth"
	"monitor-system/agent_service/internal/config"

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

const maxConcurrentAllServerAnomalyQueries = 8

var apiGatewayHTTPClient = &http.Client{
	Timeout: 10 * time.Second,
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

// NewMonitorAnomaliesTool 创建了一个工具函数，用于查询监控系统的异常记录。该工具函数根据输入参数构建查询参数，并发送 GET 请求到 API 网关的 /api/servers/{server_name}/anomalies 端点（如果指定了服务器名称）或 /api/servers/*/anomalies 端点（如果未指定服务器名称）。返回结果包含异常记录列表，支持分页和阈值过滤。
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

// NewMonitorPerformanceTool 创建了一个工具函数，用于查询监控系统的性能数据。该工具函数根据输入参数构建查询参数，并发送 GET 请求到 API 网关的 /api/servers/{server_name}/performance 端点。返回结果包含性能数据的时间序列记录，支持分页。
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

// NewMonitorTrendTool 创建了一个工具函数，用于查询监控系统的趋势数据。该工具函数根据输入参数构建查询参数，并发送 GET 请求到 API 网关的 /api/servers/{server_name}/trend 端点。返回结果包含趋势数据的时间序列记录，支持分页和趋势聚合间隔。
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

// NewMonitorDetailTool 创建了一个工具函数，用于查询监控系统的详细数据。该工具函数根据输入参数构建查询参数，并发送 GET 请求到 API 网关的 /api/servers/{server_name}/{kind}-detail 端点，其中 {kind} 可以是 net、disk、mem、softirq、mysql 或 redis。返回结果包含指定类型的详细数据记录，支持分页。
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

// NewMonitorRedisDetailTool 创建了一个工具函数，用于查询监控系统的 Redis 明细数据。该工具函数根据输入参数构建查询参数，并发送 GET 请求到 API 网关的 /api/servers/{server_name}/redis-detail 端点。返回结果包含 Redis 的可用性、连接压力、内存压力、命令执行速率、命中率、驱逐和拒绝连接数、复制延迟和慢日志增长等监控指标的详细记录，支持分页。
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

// NewMonitorMysqlDetailTool 创建了一个工具函数，用于查询监控系统的 MySQL 明细数据。该工具函数根据输入参数构建查询参数，并发送 GET 请求到 API 网关的 /api/servers/{server_name}/mysql-detail 端点。返回结果包含 MySQL 的可用性、连接压力、QPS/TPS、慢查询率、InnoDB 行锁等待、缓冲池命中率和复制延迟等监控指标的详细记录，支持分页。
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

// apiGatewayGet 是一个辅助函数，用于发送 GET 请求到 API 网关的指定路径，并返回原始 JSON 响应数据。它从配置中获取 API 网关的基础 URL，构建完整的请求 URL，并添加 Bearer Token 进行身份验证。函数处理 HTTP 响应，检查状态码，并尝试解析响应体中的通用 envelope 结构以提取数据或错误信息。
func apiGatewayGet(ctx context.Context, path string, query url.Values) (json.RawMessage, error) {
	baseURL, err := config.ConfigString(ctx, "api_gateway_base_url", "API_GATEWAY_BASE_URL", "http://127.0.0.1:8080")
	if err != nil {
		return nil, err
	}
	endpoint := buildAPIGatewayURL(baseURL, path, query)
	// 构建 HTTP GET 请求，并将上下文传递给请求以支持超时和取消。添加 Bearer Token 进行身份验证。
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return nil, err
	}
	addBearerToken(ctx, req)
	resp, err := apiGatewayHTTPClient.Do(req)
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

// 添加 Bearer Token 到 HTTP 请求头中，以便 API 网关进行身份验证。函数从上下文中提取 Bearer Token，如果存在，则将其添加到请求的 Authorization 头中
func addBearerToken(ctx context.Context, req *http.Request) {
	if token, ok := authctx.BearerTokenFromContext(ctx); ok {
		req.Header.Set("Authorization", "Bearer "+token)
	}
}

// 构建 API 网关请求 URL 的辅助函数。它接受基础 URL、路径和查询参数，确保正确拼接路径并编码查询参数，返回完整的请求 URL。
func buildAPIGatewayURL(baseURL string, path string, query url.Values) string {
	endpoint := strings.TrimRight(baseURL, "/") + path
	if len(query) > 0 {
		endpoint += "?" + query.Encode()
	}
	return endpoint
}

// queryAllServerAnomalies 是一个辅助函数，用于查询所有服务器的异常记录。当输入参数中未指定服务器名称时，工具函数会调用该函数。它首先查询最新的服务器概览以获取所有服务器名称，然后针对每个服务器发送 GET 请求到 API 网关的 /api/servers/{server_name}/anomalies 端点，并收集结果。最终返回一个包含所有服务器异常记录的 JSON 数据。
func queryAllServerAnomalies(ctx context.Context, params url.Values) (json.RawMessage, error) {
	// 1. 查询最新的服务器概览以获取所有服务器名称
	overview, err := apiGatewayGet(ctx, "/api/servers/latest", nil)
	if err != nil {
		return nil, err
	}
	// 2. 解析服务器概览响应，提取服务器名称列表
	var latest struct {
		Servers []struct {
			ServerName string `json:"server_name"`
		} `json:"servers"`
	}
	if err := json.Unmarshal(overview, &latest); err != nil {
		return nil, err
	}
	// 3. 提取有效服务器名称，结果切片按概览顺序预分配，确保并发查询后输出顺序稳定。
	serverNames := make([]string, 0, len(latest.Servers))
	for _, server := range latest.Servers {
		if server.ServerName == "" {
			continue
		}
		serverNames = append(serverNames, server.ServerName)
	}

	// 4. 针对每个服务器并发查询异常记录，并使用信号量限制并发度，避免瞬间压垮 API 网关。
	results := make([]map[string]interface{}, len(serverNames))
	sem := make(chan struct{}, maxConcurrentAllServerAnomalyQueries)
	var wg sync.WaitGroup
	for index, serverName := range serverNames {
		wg.Add(1)
		go func(index int, serverName string) {
			defer wg.Done()
			sem <- struct{}{}
			defer func() { <-sem }()
			results[index] = queryOneServerAnomalies(ctx, serverName, params)
		}(index, serverName)
	}
	wg.Wait()

	out, err := json.Marshal(map[string]interface{}{
		"servers": results,
	})
	if err != nil {
		return nil, err
	}
	return out, nil
}

func queryOneServerAnomalies(ctx context.Context, serverName string, params url.Values) map[string]interface{} {
	path := fmt.Sprintf("/api/servers/%s/anomalies", url.PathEscape(serverName))
	data, err := apiGatewayGet(ctx, path, cloneValues(params))
	result := map[string]interface{}{"server_name": serverName}
	if err != nil {
		result["success"] = false
		result["error"] = err.Error()
		return result
	}
	var payload interface{}
	if json.Unmarshal(data, &payload) == nil {
		result["success"] = true
		result["data"] = payload
		return result
	}
	result["success"] = true
	result["data"] = string(data)
	return result
}

// 构建查询异常记录的 URL 查询参数的辅助函数。
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

// formatGatewayOutput 是一个辅助函数，用于将 API 网关的响应数据和错误信息格式化为统一的工具输出结构。它接受数据来源、原始 JSON 数据和错误对象作为输入，并返回一个 JSON 字符串表示工具输出。如果发生错误，输出将包含成功标志、数据来源和错误信息；如果成功，输出将包含成功标志、数据来源、解析后的数据和消息。
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
	// 尝试将原始 JSON 数据解析为通用接口，如果解析失败，则将原始数据作为字符串返回。这允许工具输出在数据格式不确定的情况下仍然提供有用的信息。
	if len(data) > 0 && json.Unmarshal(data, &payload) != nil {
		payload = string(data)
	}
	// 构建工具输出结构，并将其序列化为 JSON 字符串返回。输出包含成功标志、数据来源、解析后的数据和消息。如果序列化过程中发生错误，则返回该错误。
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

// 深拷贝 url.Values 以避免在循环中修改原始查询参数时引发竞态条件。该函数创建一个新的 url.Values 实例，并将原始值逐个复制到新实例中，确保每个服务器的查询参数独立且不会相互干扰。
func cloneValues(values url.Values) url.Values {
	cloned := url.Values{}
	for key, list := range values {
		for _, value := range list {
			cloned.Add(key, value)
		}
	}
	return cloned
}

// errorTool 是一个辅助函数，用于在工具函数初始化失败时创建一个返回固定错误信息的工具。该工具函数使用 utils.InferOptionableTool 创建，工具名称和描述由参数指定，执行函数始终返回一个格式化的错误输出，指示工具初始化失败并包含原始构建错误信息。这确保了即使在工具构建过程中发生错误，系统仍然能够提供有意义的反馈，而不是完全无法调用该工具。
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
