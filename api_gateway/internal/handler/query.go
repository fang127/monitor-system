package handler

import (
	"context"
	"encoding/json"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"monitor-system/api_gateway/internal/grpcclient"
	"monitor-system/api_gateway/internal/response"
)

// defaultQueryWindow 定义了默认的查询时间范围（1小时），如果用户没有提供start_time和end_time参数，系统将使用当前时间向前推一个小时作为查询范围。
const defaultQueryWindow = time.Hour

type QueryHandler struct {
	client *grpcclient.Client // gRPC客户端，用于调用后端服务
}

func NewQueryHandler(client *grpcclient.Client) *QueryHandler {
	return &QueryHandler{client: client}
}

// Latest 处理查询最新监控数据的请求
func (h *QueryHandler) Latest(c *gin.Context) {
	data, err := h.client.Latest(c.Request.Context())
	writeGRPCResult(c, data, err)
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

	data, err := h.client.Performance(c.Request.Context(), c.Param("server"), grpcclient.PerformanceOptions{
		TimeRange: timeRange,
		Page:      page,
		PageSize:  pageSize,
	})
	writeGRPCResult(c, data, err)
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

	data, err := h.client.Trend(c.Request.Context(), c.Param("server"), grpcclient.TrendOptions{
		TimeRange:       timeRange,
		IntervalSeconds: intervalSeconds,
	})
	writeGRPCResult(c, data, err)
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

	data, err := h.client.Anomalies(c.Request.Context(), c.Param("server"), grpcclient.AnomalyOptions{
		TimeRange:           timeRange,
		CPUThreshold:        cpuThreshold,
		MemThreshold:        memThreshold,
		DiskThreshold:       diskThreshold,
		ChangeRateThreshold: changeRateThreshold,
		Page:                page,
		PageSize:            pageSize,
	})
	writeGRPCResult(c, data, err)
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

	data, err := h.client.ScoreRank(c.Request.Context(), grpcclient.ScoreRankOptions{
		Order:    order,
		Page:     page,
		PageSize: pageSize,
	})
	writeGRPCResult(c, data, err)
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

func (h *QueryHandler) detail(c *gin.Context, call func(ctx context.Context, server string, opts grpcclient.DetailOptions) (json.RawMessage, error)) {
	timeRange, ok := parseTimeRange(c)
	if !ok {
		return
	}
	page, pageSize, ok := parsePagination(c)
	if !ok {
		return
	}

	data, err := call(c.Request.Context(), c.Param("server"), grpcclient.DetailOptions{
		TimeRange: timeRange,
		Page:      page,
		PageSize:  pageSize,
	})
	writeGRPCResult(c, data, err)
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

func parseQueryTime(value string) (time.Time, error) {
	if unixSeconds, err := strconv.ParseInt(value, 10, 64); err == nil {
		return time.Unix(unixSeconds, 0), nil
	}
	return time.Parse(time.RFC3339, value)
}

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

func writeGRPCResult(c *gin.Context, data json.RawMessage, err error) {
	if err != nil {
		response.Error(c, grpcHTTPStatus(err), err.Error())
		return
	}
	response.OK(c, http.StatusOK, data)
}

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
	default:
		return http.StatusInternalServerError
	}
}
