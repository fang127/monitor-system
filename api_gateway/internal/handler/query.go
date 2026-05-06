package handler

import (
	"encoding/json"
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"monitor-system/api_gateway/internal/grpcclient"
	"monitor-system/api_gateway/internal/response"
)

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
