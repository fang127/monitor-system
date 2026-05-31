package handler

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"monitor-system/api_gateway/internal/auth"
	"monitor-system/api_gateway/internal/grpcclient"
	"monitor-system/api_gateway/internal/response"
)

// defaultQueryWindow 定义了默认的查询时间范围（1小时），如果用户没有提供start_time和end_time参数，系统将使用当前时间向前推一个小时作为查询范围。
const defaultQueryWindow = time.Hour

type QueryHandler struct {
	client      *grpcclient.Client // gRPC客户端，用于调用后端服务
	managerAddr string             // Manager gRPC地址，用于生成可诊断的错误信息
}

func NewQueryHandler(client *grpcclient.Client, managerAddr string) *QueryHandler {
	return &QueryHandler{client: client, managerAddr: managerAddr}
}

// Latest 处理查询最新监控数据的请求
func (h *QueryHandler) Latest(c *gin.Context) {
	ctx, ok := h.scopedContext(c)
	if !ok {
		return
	}
	data, err := h.client.Latest(ctx)
	h.writeGRPCResult(c, data, err)
}

// Performance 处理查询历史性能数据的请求
func (h *QueryHandler) Performance(c *gin.Context) {
	timeRange, ok := parseTimeRange(c)
	if !ok {
		return
	}
	page, pageSize, ok := parsePagination(c)
	if !ok {
		return
	}
	ctx, ok := h.scopedContext(c)
	if !ok {
		return
	}

	data, err := h.client.Performance(ctx, c.Param("server"), grpcclient.PerformanceOptions{
		TimeRange: timeRange,
		Page:      page,
		PageSize:  pageSize,
	})
	h.writeGRPCResult(c, data, err)
}

// Trend 处理查询监控数据趋势的请求，支持可选的时间范围和时间间隔参数
func (h *QueryHandler) Trend(c *gin.Context) {
	timeRange, ok := parseTimeRange(c)
	if !ok {
		return
	}

	intervalSeconds, ok := parseOptionalInt32(c, "interval_seconds", 0)
	if !ok {
		return
	}
	ctx, ok := h.scopedContext(c)
	if !ok {
		return
	}

	data, err := h.client.Trend(ctx, c.Param("server"), grpcclient.TrendOptions{
		TimeRange:       timeRange,
		IntervalSeconds: intervalSeconds,
	})
	h.writeGRPCResult(c, data, err)
}

// Anomalies 处理查询监控数据异常的请求，支持可选的时间范围、分页参数和阈值参数
func (h *QueryHandler) Anomalies(c *gin.Context) {
	timeRange, ok := parseTimeRange(c)
	if !ok {
		return
	}

	page, ok := parseOptionalInt32(c, "page", 1)
	if !ok {
		return
	}
	pageSize, ok := parseOptionalInt32(c, "page_size", 100)
	if !ok {
		return
	}
	cpuThreshold, ok := parseOptionalFloat32(c, "cpu_threshold", 0)
	if !ok {
		return
	}
	memThreshold, ok := parseOptionalFloat32(c, "mem_threshold", 0)
	if !ok {
		return
	}
	diskThreshold, ok := parseOptionalFloat32(c, "disk_threshold", 0)
	if !ok {
		return
	}
	changeRateThreshold, ok := parseOptionalFloat32(c, "change_rate_threshold", 0)
	if !ok {
		return
	}
	mysqlConnectionThreshold, ok := parseOptionalFloat32(c, "mysql_connection_threshold", 0)
	if !ok {
		return
	}
	mysqlReplicationLagThreshold, ok := parseOptionalFloat32(c, "mysql_replication_lag_threshold", 0)
	if !ok {
		return
	}
	mysqlSlowQueryRateThreshold, ok := parseOptionalFloat32(c, "mysql_slow_query_rate_threshold", 0)
	if !ok {
		return
	}
	mysqlLockWaitRateThreshold, ok := parseOptionalFloat32(c, "mysql_lock_wait_rate_threshold", 0)
	if !ok {
		return
	}
	mysqlBufferPoolHitThreshold, ok := parseOptionalFloat32(c, "mysql_buffer_pool_hit_threshold", 0)
	if !ok {
		return
	}
	redisConnectionThreshold, ok := parseOptionalFloat32(c, "redis_connection_threshold", 0)
	if !ok {
		return
	}
	redisMemoryThreshold, ok := parseOptionalFloat32(c, "redis_memory_threshold", 0)
	if !ok {
		return
	}
	redisHitRateThreshold, ok := parseOptionalFloat32(c, "redis_hit_rate_threshold", 0)
	if !ok {
		return
	}
	redisReplicationLagThreshold, ok := parseOptionalFloat32(c, "redis_replication_lag_threshold", 0)
	if !ok {
		return
	}
	redisSlowlogGrowthThreshold, ok := parseOptionalFloat32(c, "redis_slowlog_growth_threshold", 0)
	if !ok {
		return
	}
	ctx, ok := h.scopedContext(c)
	if !ok {
		return
	}

	data, err := h.client.Anomalies(ctx, c.Param("server"), grpcclient.AnomalyOptions{
		TimeRange:                    timeRange,
		CPUThreshold:                 cpuThreshold,
		MemThreshold:                 memThreshold,
		DiskThreshold:                diskThreshold,
		ChangeRateThreshold:          changeRateThreshold,
		MySQLConnectionThreshold:     mysqlConnectionThreshold,
		MySQLReplicationLagThreshold: mysqlReplicationLagThreshold,
		MySQLSlowQueryRateThreshold:  mysqlSlowQueryRateThreshold,
		MySQLLockWaitRateThreshold:   mysqlLockWaitRateThreshold,
		MySQLBufferPoolHitThreshold:  mysqlBufferPoolHitThreshold,
		RedisConnectionThreshold:     redisConnectionThreshold,
		RedisMemoryThreshold:         redisMemoryThreshold,
		RedisHitRateThreshold:        redisHitRateThreshold,
		RedisReplicationLagThreshold: redisReplicationLagThreshold,
		RedisSlowlogGrowthThreshold:  redisSlowlogGrowthThreshold,
		Page:                         page,
		PageSize:                     pageSize,
	})
	h.writeGRPCResult(c, data, err)
}

// ScoreRank 处理服务器评分排序查询请求
func (h *QueryHandler) ScoreRank(c *gin.Context) {
	page, pageSize, ok := parsePagination(c)
	if !ok {
		return
	}
	order, ok := parseSortOrder(c)
	if !ok {
		return
	}
	ctx, ok := h.scopedContext(c)
	if !ok {
		return
	}

	data, err := h.client.ScoreRank(ctx, grpcclient.ScoreRankOptions{
		Order:    order,
		Page:     page,
		PageSize: pageSize,
	})
	h.writeGRPCResult(c, data, err)
}

// NetDetail 处理网络详细指标查询请求
func (h *QueryHandler) NetDetail(c *gin.Context) {
	h.detail(c, h.client.NetDetail)
}

// DiskDetail 处理磁盘详细指标查询请求
func (h *QueryHandler) DiskDetail(c *gin.Context) {
	h.detail(c, h.client.DiskDetail)
}

// MemDetail 处理内存详细指标查询请求
func (h *QueryHandler) MemDetail(c *gin.Context) {
	h.detail(c, h.client.MemDetail)
}

// SoftIrqDetail 处理软中断详细指标查询请求
func (h *QueryHandler) SoftIrqDetail(c *gin.Context) {
	h.detail(c, h.client.SoftIrqDetail)
}

// MysqlDetail 处理 MySQL 详细指标查询请求
func (h *QueryHandler) MysqlDetail(c *gin.Context) {
	h.detail(c, h.client.MysqlDetail)
}

// RedisDetail 处理 Redis 详细指标查询请求
func (h *QueryHandler) RedisDetail(c *gin.Context) {
	h.detail(c, h.client.RedisDetail)
}

// detail 是一个通用的处理函数，用于处理网络、磁盘、内存和软中断等详细指标的查询请求。它首先解析时间范围和分页参数，然后调用传入的gRPC函数获取数据，并将结果写入HTTP响应。
func (h *QueryHandler) detail(c *gin.Context, call func(ctx context.Context, server string, opts grpcclient.DetailOptions) (json.RawMessage, error)) {
	timeRange, ok := parseTimeRange(c)
	if !ok {
		return
	}
	page, pageSize, ok := parsePagination(c)
	if !ok {
		return
	}
	ctx, ok := h.scopedContext(c)
	if !ok {
		return
	}

	data, err := call(ctx, c.Param("server"), grpcclient.DetailOptions{
		TimeRange: timeRange,
		Page:      page,
		PageSize:  pageSize,
	})
	h.writeGRPCResult(c, data, err)
}

// scopedContext 从HTTP请求中提取认证信息，并根据查询参数中的租户ID、团队ID和集群ID构建一个带有访问范围的gRPC上下文。如果认证信息缺失或查询参数中的范围不合法，函数会直接返回错误响应并返回false。
func (h *QueryHandler) scopedContext(c *gin.Context) (context.Context, bool) {
	claims, ok := auth.CurrentClaims(c)
	if !ok {
		response.Error(c, http.StatusUnauthorized, "未登录或登录已过期")
		return nil, false
	}
	if tenantID := strings.TrimSpace(c.Query("tenant_id")); tenantID != "" && tenantID != claims.TenantID {
		response.Error(c, http.StatusForbidden, "无权访问指定租户的监控数据")
		return nil, false
	}
	if teamID := strings.TrimSpace(c.Query("team_id")); teamID != "" && teamID != claims.TeamID {
		response.Error(c, http.StatusForbidden, "无权访问指定团队的监控数据")
		return nil, false
	}
	return grpcclient.ContextWithScope(c.Request.Context(), grpcclient.Scope{
		TenantID:  claims.TenantID,
		TeamID:    claims.TeamID,
		ClusterID: strings.TrimSpace(c.Query("cluster_id")),
	}), true
}

// parseTimeRange 从查询参数中解析时间范围，支持两种格式：
// 1. Unix时间戳（秒）
// 2. RFC3339格式的时间字符串
// 如果参数无效，函数会直接返回错误响应并返回false。
// 如果参数缺失，函数会使用默认的时间范围（当前时间向前推一个小时）。
func parseTimeRange(c *gin.Context) (grpcclient.TimeRange, bool) {
	end := time.Now()
	start := end.Add(-defaultQueryWindow)

	if value := c.Query("end_time"); value != "" {
		parsed, err := parseQueryTime(value)
		if err != nil {
			response.Error(c, http.StatusBadRequest, "invalid end_time")
			return grpcclient.TimeRange{}, false
		}
		end = parsed
	}

	if value := c.Query("start_time"); value != "" {
		parsed, err := parseQueryTime(value)
		if err != nil {
			response.Error(c, http.StatusBadRequest, "invalid start_time")
			return grpcclient.TimeRange{}, false
		}
		start = parsed
	}

	if start.After(end) {
		response.Error(c, http.StatusBadRequest, "start_time must be before end_time")
		return grpcclient.TimeRange{}, false
	}

	return grpcclient.TimeRange{Start: start, End: end}, true
}

// parseQueryTime 尝试将查询参数解析为时间，首先尝试将其解析为Unix时间戳（秒），如果失败则尝试解析为RFC3339格式的时间字符串。如果两种解析方式都失败，函数会返回错误。
func parseQueryTime(value string) (time.Time, error) {
	if unixSeconds, err := strconv.ParseInt(value, 10, 64); err == nil {
		return time.Unix(unixSeconds, 0), nil
	}
	return time.Parse(time.RFC3339, value)
}

// parsePagination 解析分页参数，支持page和page_size两个参数，默认值分别为1和100。如果参数无效，函数会直接返回错误响应并返回false。
func parsePagination(c *gin.Context) (int32, int32, bool) {
	page, ok := parseOptionalInt32(c, "page", 1)
	if !ok {
		return 0, 0, false
	}
	pageSize, ok := parseOptionalInt32(c, "page_size", 100)
	if !ok {
		return 0, 0, false
	}
	return page, pageSize, true
}

// parseSortOrder 解析排序顺序参数，支持"asc"和"desc"，默认值为"desc"。如果参数无效，函数会直接返回错误响应并返回false。
func parseSortOrder(c *gin.Context) (int32, bool) {
	switch strings.ToLower(c.DefaultQuery("order", "desc")) {
	case "desc":
		return 0, true
	case "asc":
		return 1, true
	default:
		response.Error(c, http.StatusBadRequest, "invalid order")
		return 0, false
	}
}

// parseOptionalInt32 解析可选的整数查询参数，如果参数缺失则返回默认值，如果参数无效则返回错误响应。
func parseOptionalInt32(c *gin.Context, key string, fallback int32) (int32, bool) {
	value := c.Query(key)
	if value == "" {
		return fallback, true
	}
	parsed, err := strconv.ParseInt(value, 10, 32)
	if err != nil {
		response.Error(c, http.StatusBadRequest, "invalid "+key)
		return 0, false
	}
	return int32(parsed), true
}

// parseOptionalFloat32 解析可选的浮点数查询参数，如果参数缺失则返回默认值，如果参数无效则返回错误响应。
func parseOptionalFloat32(c *gin.Context, key string, fallback float32) (float32, bool) {
	value := c.Query(key)
	if value == "" {
		return fallback, true
	}
	parsed, err := strconv.ParseFloat(value, 32)
	if err != nil {
		response.Error(c, http.StatusBadRequest, "invalid "+key)
		return 0, false
	}
	return float32(parsed), true
}

// writeGRPCResult 将gRPC调用的结果写入HTTP响应，如果gRPC调用返回错误，则根据错误类型返回相应的HTTP状态码和错误消息。
func (h *QueryHandler) writeGRPCResult(c *gin.Context, data json.RawMessage, err error) {
	if err != nil {
		response.Error(c, grpcHTTPStatus(err), h.grpcErrorMessage(err))
		return
	}
	response.OK(c, http.StatusOK, data)
}

func (h *QueryHandler) grpcErrorMessage(err error) string {
	if status.Code(err) != codes.Unavailable {
		return err.Error()
	}
	return fmt.Sprintf(
		"manager gRPC unavailable at %s; please start manager and verify MANAGER_GRPC_ADDR/DOCKER_MANAGER_GRPC_ADDR. detail: %v",
		h.managerAddr,
		err,
	)
}

// grpcHTTPStatus 根据gRPC错误的状态码映射到相应的HTTP状态码，常见的映射包括：
// - InvalidArgument -> 400 Bad Request
// - NotFound -> 404 Not Found
// - DeadlineExceeded -> 504 Gateway Timeout
// - Unavailable -> 502 Bad Gateway
// - ResourceExhausted -> 429 Too Many Requests
// 对于其他未明确映射的错误，默认返回500 Internal Server Error。
func grpcHTTPStatus(err error) int {
	switch status.Code(err) {
	case codes.InvalidArgument:
		return http.StatusBadRequest
	case codes.NotFound:
		return http.StatusNotFound
	case codes.DeadlineExceeded:
		return http.StatusGatewayTimeout
	case codes.Unavailable:
		return http.StatusBadGateway
	case codes.ResourceExhausted:
		return http.StatusTooManyRequests
	default:
		return http.StatusInternalServerError
	}
}
