package grpcclient

import (
	"context"
	"encoding/json"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/types/dynamicpb"
	"google.golang.org/protobuf/types/known/timestamppb"
)

// grpc服务全名，格式为 /package.service/method
const queryServiceFullName = "/monitor.proto.QueryService/"

type Client struct {
	conn    *grpc.ClientConn
	timeout time.Duration
}

type TimeRange struct {
	Start time.Time
	End   time.Time
}

// TrendOptions 包含查询趋势图的选项
type TrendOptions struct {
	TimeRange       TimeRange // 查询的时间范围
	IntervalSeconds int32     // 趋势图的时间间隔，单位为秒，默认为0表示自动选择
}

// AnomalyOptions 包含查询异常数据的选项
type AnomalyOptions struct {
	TimeRange           TimeRange // 查询的时间范围
	CPUThreshold        float32   // CPU使用率的异常阈值，默认为0表示不使用
	MemThreshold        float32   // 内存使用率的异常阈值，默认为0表示不使用
	DiskThreshold       float32   // 磁盘使用率的异常阈值，默认为0表示不使用
	ChangeRateThreshold float32   // 变化率的异常阈值，默认为0表示不使用
	Page                int32     // 分页页码，默认为0表示不分页
	PageSize            int32     // 分页大小，默认为0表示不分页
}

func New(address string, timeout time.Duration) (*Client, error) {
	if timeout <= 0 {
		timeout = 5 * time.Second
	}
	// 使用不安全的连接选项，适用于本地开发和测试环境
	conn, err := grpc.NewClient(address, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, err
	}
	return &Client{conn: conn, timeout: timeout}, nil
}

func (c *Client) Close() error {
	return c.conn.Close()
}

// Latest 获取最新的服务器性能数据
func (c *Client) Latest(ctx context.Context) (json.RawMessage, error) {
	req, err := newMessage("QueryLatestScoreRequest")
	if err != nil {
		return nil, err
	}
	resp, err := newMessage("QueryLatestScoreResponse")
	if err != nil {
		return nil, err
	}
	return c.invokeJSON(ctx, "QueryLatestScore", req, resp)
}

// Trend 获取服务器性能的趋势数据
func (c *Client) Trend(ctx context.Context, server string, opts TrendOptions) (json.RawMessage, error) {
	req, err := newMessage("QueryTrendRequest")
	if err != nil {
		return nil, err
	}
	setString(req, "server_name", server)
	setTimeRange(req, "time_range", opts.TimeRange)
	if opts.IntervalSeconds > 0 {
		setInt32(req, "interval_seconds", opts.IntervalSeconds)
	}

	resp, err := newMessage("QueryTrendResponse")
	if err != nil {
		return nil, err
	}
	return c.invokeJSON(ctx, "QueryTrend", req, resp)
}

// Anomalies 获取服务器性能的异常数据
func (c *Client) Anomalies(ctx context.Context, server string, opts AnomalyOptions) (json.RawMessage, error) {
	req, err := newMessage("QueryAnomalyRequest")
	if err != nil {
		return nil, err
	}
	setString(req, "server_name", server)
	setTimeRange(req, "time_range", opts.TimeRange)
	setFloat32(req, "cpu_threshold", opts.CPUThreshold)
	setFloat32(req, "mem_threshold", opts.MemThreshold)
	setFloat32(req, "disk_threshold", opts.DiskThreshold)
	setFloat32(req, "change_rate_threshold", opts.ChangeRateThreshold)
	setPagination(req, "pagination", opts.Page, opts.PageSize)

	resp, err := newMessage("QueryAnomalyResponse")
	if err != nil {
		return nil, err
	}
	return c.invokeJSON(ctx, "QueryAnomaly", req, resp)
}

// invokeJSON 调用gRPC方法并将响应转换为JSON格式
func (c *Client) invokeJSON(ctx context.Context, method string, req *dynamicpb.Message, resp *dynamicpb.Message) (json.RawMessage, error) {
	ctx, cancel := context.WithTimeout(ctx, c.timeout)
	defer cancel()

	if err := c.conn.Invoke(ctx, queryServiceFullName+method, req, resp); err != nil {
		return nil, err
	}

	data, err := protojson.MarshalOptions{
		EmitUnpopulated: true,
		UseProtoNames:   true,
	}.Marshal(resp)
	if err != nil {
		return nil, err
	}
	return json.RawMessage(data), nil
}

// newMessage 根据消息名称创建一个动态消息实例
func newMessage(name protoreflect.Name) (*dynamicpb.Message, error) {
	// 通过消息名称获取消息描述符
	desc, err := messageDescriptor(name)
	if err != nil {
		return nil, err
	}
	// 创建一个动态消息实例
	return dynamicpb.NewMessage(desc), nil
}

// messageDescriptor 根据消息名称获取消息描述符
func setString(msg *dynamicpb.Message, name protoreflect.Name, value string) {
	msg.Set(msg.Descriptor().Fields().ByName(name), protoreflect.ValueOfString(value))
}

func setInt32(msg *dynamicpb.Message, name protoreflect.Name, value int32) {
	msg.Set(msg.Descriptor().Fields().ByName(name), protoreflect.ValueOfInt32(value))
}

func setFloat32(msg *dynamicpb.Message, name protoreflect.Name, value float32) {
	if value > 0 {
		msg.Set(msg.Descriptor().Fields().ByName(name), protoreflect.ValueOfFloat32(value))
	}
}

func setTimeRange(msg *dynamicpb.Message, name protoreflect.Name, value TimeRange) {
	field := msg.Descriptor().Fields().ByName(name)
	rangeMsg := dynamicpb.NewMessage(field.Message())
	setTimestamp(rangeMsg, "start_time", value.Start)
	setTimestamp(rangeMsg, "end_time", value.End)
	msg.Set(field, protoreflect.ValueOfMessage(rangeMsg))
}

func setTimestamp(msg *dynamicpb.Message, name protoreflect.Name, value time.Time) {
	field := msg.Descriptor().Fields().ByName(name)
	msg.Set(field, protoreflect.ValueOfMessage(timestamppb.New(value).ProtoReflect()))
}

// setPagination 设置分页参数
func setPagination(msg *dynamicpb.Message, name protoreflect.Name, page int32, pageSize int32) {
	field := msg.Descriptor().Fields().ByName(name)
	pagination := dynamicpb.NewMessage(field.Message())
	if page > 0 {
		setInt32(pagination, "page", page)
	}
	if pageSize > 0 {
		setInt32(pagination, "page_size", pageSize)
	}
	msg.Set(field, protoreflect.ValueOfMessage(pagination))
}
