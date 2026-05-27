package gateway

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"
)

type Client struct {
	baseURL string
	http    *http.Client
}

type Query struct {
	StartTime       string
	EndTime         string
	Page            int
	PageSize        int
	IntervalSeconds int
}

type ServerSummary struct {
	ServerName      string  `json:"server_name"`
	Score           float64 `json:"score"`
	LastUpdate      string  `json:"last_update"`
	Status          any     `json:"status"`
	CPUPercent      float64 `json:"cpu_percent"`
	MemUsedPercent  float64 `json:"mem_used_percent"`
	DiskUtilPercent float64 `json:"disk_util_percent"`
	LoadAvg1        float64 `json:"load_avg_1"`
}

type LatestResponse struct {
	Servers      []ServerSummary `json:"servers"`
	ClusterStats any             `json:"cluster_stats"`
}

type envelope struct {
	Code    int             `json:"code"`
	Message string          `json:"message"`
	Data    json.RawMessage `json:"data"`
}

func NewClient(baseURL string, httpClient *http.Client) *Client {
	if httpClient == nil {
		httpClient = &http.Client{Timeout: 10 * time.Second}
	}
	if strings.TrimSpace(baseURL) == "" {
		baseURL = "http://127.0.0.1:8080"
	}
	return &Client{baseURL: strings.TrimRight(baseURL, "/"), http: httpClient}
}

func (c *Client) Latest(ctx context.Context) (json.RawMessage, error) {
	return c.get(ctx, "/api/servers/latest", nil)
}

func (c *Client) LatestTyped(ctx context.Context) (LatestResponse, json.RawMessage, error) {
	raw, err := c.Latest(ctx)
	if err != nil {
		return LatestResponse{}, nil, err
	}
	var latest LatestResponse
	if err := json.Unmarshal(raw, &latest); err != nil {
		return LatestResponse{}, raw, err
	}
	return latest, raw, nil
}

func (c *Client) ScoreRank(ctx context.Context, q Query) (json.RawMessage, error) {
	return c.get(ctx, "/api/servers/score-rank", queryValues(q, false, true))
}

func (c *Client) Anomalies(ctx context.Context, server string, q Query) (json.RawMessage, error) {
	return c.get(ctx, fmt.Sprintf("/api/servers/%s/anomalies", url.PathEscape(server)), queryValues(q, false, true))
}

func (c *Client) Performance(ctx context.Context, server string, q Query) (json.RawMessage, error) {
	return c.get(ctx, fmt.Sprintf("/api/servers/%s/performance", url.PathEscape(server)), queryValues(q, false, true))
}

func (c *Client) Trend(ctx context.Context, server string, q Query) (json.RawMessage, error) {
	return c.get(ctx, fmt.Sprintf("/api/servers/%s/trend", url.PathEscape(server)), queryValues(q, true, false))
}

func (c *Client) Detail(ctx context.Context, server, kind string, q Query) (json.RawMessage, error) {
	endpoint, err := detailEndpoint(kind)
	if err != nil {
		return nil, err
	}
	return c.get(ctx, fmt.Sprintf("/api/servers/%s/%s", url.PathEscape(server), endpoint), queryValues(q, false, true))
}

func (c *Client) MysqlDetail(ctx context.Context, server string, q Query) (json.RawMessage, error) {
	return c.Detail(ctx, server, "mysql", q)
}

func (c *Client) RedisDetail(ctx context.Context, server string, q Query) (json.RawMessage, error) {
	return c.Detail(ctx, server, "redis", q)
}

func (c *Client) get(ctx context.Context, path string, values url.Values) (json.RawMessage, error) {
	endpoint := c.baseURL + path
	if len(values) > 0 {
		endpoint += "?" + values.Encode()
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return nil, err
	}
	resp, err := c.http.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("api_gateway returned status %d: %s", resp.StatusCode, strings.TrimSpace(string(body)))
	}
	var env envelope
	if err := json.Unmarshal(body, &env); err == nil && env.Message != "" {
		if env.Code != 0 {
			return nil, fmt.Errorf("api_gateway error %d: %s", env.Code, env.Message)
		}
		if len(env.Data) == 0 {
			return json.RawMessage(`{}`), nil
		}
		return env.Data, nil
	}
	return body, nil
}

func queryValues(q Query, interval bool, pagination bool) url.Values {
	values := url.Values{}
	if q.StartTime != "" {
		values.Set("start_time", q.StartTime)
	}
	if q.EndTime != "" {
		values.Set("end_time", q.EndTime)
	}
	if pagination {
		if q.Page > 0 {
			values.Set("page", fmt.Sprintf("%d", q.Page))
		}
		if q.PageSize > 0 {
			values.Set("page_size", fmt.Sprintf("%d", q.PageSize))
		}
	}
	if interval && q.IntervalSeconds > 0 {
		values.Set("interval_seconds", fmt.Sprintf("%d", q.IntervalSeconds))
	}
	return values
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
	case "mysql", "db":
		return "mysql-detail", nil
	case "redis", "cache":
		return "redis-detail", nil
	default:
		return "", fmt.Errorf("unsupported detail kind %q", kind)
	}
}
