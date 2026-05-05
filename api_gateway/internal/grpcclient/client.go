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

const queryServiceFullName = "/monitor.proto.QueryService/"

type Client struct {
	conn    *grpc.ClientConn
	timeout time.Duration
}

type TimeRange struct {
	Start time.Time
	End   time.Time
}

type TrendOptions struct {
	TimeRange       TimeRange
	IntervalSeconds int32
}

type AnomalyOptions struct {
	TimeRange           TimeRange
	CPUThreshold        float32
	MemThreshold        float32
	DiskThreshold       float32
	ChangeRateThreshold float32
	Page                int32
	PageSize            int32
}

func New(address string, timeout time.Duration) (*Client, error) {
	if timeout <= 0 {
		timeout = 5 * time.Second
	}
	conn, err := grpc.Dial(address, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, err
	}
	return &Client{conn: conn, timeout: timeout}, nil
}

func (c *Client) Close() error {
	return c.conn.Close()
}

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

func newMessage(name protoreflect.Name) (*dynamicpb.Message, error) {
	desc, err := messageDescriptor(name)
	if err != nil {
		return nil, err
	}
	return dynamicpb.NewMessage(desc), nil
}

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
