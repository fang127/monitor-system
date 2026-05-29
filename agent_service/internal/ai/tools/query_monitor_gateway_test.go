package tools

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	authctx "monitor-system/agent_service/internal/auth"
)

func TestDetailEndpointMapsAliases(t *testing.T) {
	cases := map[string]string{
		"net":          "net-detail",
		"network":      "net-detail",
		"memory":       "mem-detail",
		"soft_irq":     "softirq-detail",
		"mysql_detail": "mysql-detail",
		"cache":        "redis-detail",
	}

	for input, want := range cases {
		got, err := detailEndpoint(input)
		if err != nil {
			t.Fatalf("kind=%s 不应失败: %v", input, err)
		}
		if got != want {
			t.Fatalf("kind=%s got=%s want=%s", input, got, want)
		}
	}
}

func TestSeriesQueryKeepsPaginationAndIntervalRules(t *testing.T) {
	input := &MonitorSeriesInput{
		StartTime:       "2026-05-28T10:00:00Z",
		EndTime:         "2026-05-28T11:00:00Z",
		Page:            2,
		PageSize:        20,
		IntervalSeconds: 60,
	}

	performance := seriesQuery(input, true, false)
	if performance.Get("page") != "2" || performance.Get("page_size") != "20" {
		t.Fatalf("性能查询分页参数不正确: %s", performance.Encode())
	}
	if performance.Has("interval_seconds") {
		t.Fatalf("性能查询不应携带 interval_seconds: %s", performance.Encode())
	}

	trend := seriesQuery(input, false, true)
	if trend.Has("page") || trend.Has("page_size") {
		t.Fatalf("趋势查询不应携带分页参数: %s", trend.Encode())
	}
	if trend.Get("interval_seconds") != "60" {
		t.Fatalf("趋势查询 interval_seconds 不正确: %s", trend.Encode())
	}
}

func TestBuildAPIGatewayURLTrimsBaseSlashAndEncodesQuery(t *testing.T) {
	values := url.Values{}
	values.Set("page", "3")
	values.Set("page_size", "10")

	got := buildAPIGatewayURL("http://api-gateway/", "/api/servers/node-1/performance", values)
	want := "http://api-gateway/api/servers/node-1/performance?page=3&page_size=10"
	if got != want {
		t.Fatalf("URL 拼接不正确: got=%s want=%s", got, want)
	}
}

func TestAddBearerTokenForwardsTokenFromContext(t *testing.T) {
	ctx := authctx.ContextWithBearerToken(context.Background(), "user-token")
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, "http://api-gateway/api/servers/latest", nil)
	if err != nil {
		t.Fatalf("创建请求失败: %v", err)
	}
	addBearerToken(ctx, req)
	if got := req.Header.Get("Authorization"); got != "Bearer user-token" {
		t.Fatalf("Authorization = %q, want Bearer user-token", got)
	}
}

func TestAPIGatewayGetReturnsEnvelopeData(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/api/servers/latest" {
			http.Error(w, "请求路径不正确: "+r.URL.Path, http.StatusBadRequest)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{"code":0,"message":"ok","data":{"ok":true}}`))
	}))
	defer server.Close()
	t.Setenv("API_GATEWAY_BASE_URL", server.URL)

	data, err := apiGatewayGet(context.Background(), "/api/servers/latest", nil)
	if err != nil {
		t.Fatalf("apiGatewayGet 不应失败: %v", err)
	}
	if strings.TrimSpace(string(data)) != `{"ok":true}` {
		t.Fatalf("返回数据不正确: %s", data)
	}
}

func TestQueryAllServerAnomaliesRunsConcurrentlyAndKeepsOrder(t *testing.T) {
	var inFlight int64
	var maxInFlight int64
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		switch r.URL.Path {
		case "/api/servers/latest":
			_, _ = w.Write([]byte(`{"code":0,"message":"ok","data":{"servers":[{"server_name":"node-1"},{"server_name":"node-2"},{"server_name":"node-3"}]}}`))
		case "/api/servers/node-1/anomalies", "/api/servers/node-2/anomalies", "/api/servers/node-3/anomalies":
			current := atomic.AddInt64(&inFlight, 1)
			for {
				previous := atomic.LoadInt64(&maxInFlight)
				if current <= previous || atomic.CompareAndSwapInt64(&maxInFlight, previous, current) {
					break
				}
			}
			time.Sleep(30 * time.Millisecond)
			atomic.AddInt64(&inFlight, -1)
			_, _ = w.Write([]byte(`{"code":0,"message":"ok","data":{"items":[]}}`))
		default:
			http.NotFound(w, r)
		}
	}))
	defer server.Close()
	t.Setenv("API_GATEWAY_BASE_URL", server.URL)

	data, err := queryAllServerAnomalies(context.Background(), url.Values{})
	if err != nil {
		t.Fatalf("queryAllServerAnomalies 不应失败: %v", err)
	}
	if atomic.LoadInt64(&maxInFlight) < 2 {
		t.Fatalf("异常查询没有并发执行，最大并发数=%d", maxInFlight)
	}

	var output struct {
		Servers []struct {
			ServerName string `json:"server_name"`
			Success    bool   `json:"success"`
		} `json:"servers"`
	}
	if err := json.Unmarshal(data, &output); err != nil {
		t.Fatalf("解析输出失败: %v", err)
	}
	wantOrder := []string{"node-1", "node-2", "node-3"}
	if len(output.Servers) != len(wantOrder) {
		t.Fatalf("服务器数量不正确: got=%d want=%d, data=%s", len(output.Servers), len(wantOrder), data)
	}
	for i, want := range wantOrder {
		if output.Servers[i].ServerName != want {
			t.Fatalf("服务器顺序不正确: index=%d got=%s want=%s", i, output.Servers[i].ServerName, want)
		}
		if !output.Servers[i].Success {
			t.Fatalf("服务器 %s 查询应成功: data=%s", want, data)
		}
	}
}

func TestQueryAllServerAnomaliesKeepsPartialFailures(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		switch r.URL.Path {
		case "/api/servers/latest":
			_, _ = w.Write([]byte(`{"code":0,"message":"ok","data":{"servers":[{"server_name":"node-ok"},{"server_name":"node-fail"}]}}`))
		case "/api/servers/node-ok/anomalies":
			_, _ = w.Write([]byte(`{"code":0,"message":"ok","data":{"items":[{"id":1}]}}`))
		case "/api/servers/node-fail/anomalies":
			http.Error(w, `{"error":"boom"}`, http.StatusInternalServerError)
		default:
			http.NotFound(w, r)
		}
	}))
	defer server.Close()
	t.Setenv("API_GATEWAY_BASE_URL", server.URL)

	data, err := queryAllServerAnomalies(context.Background(), url.Values{})
	if err != nil {
		t.Fatalf("queryAllServerAnomalies 不应因单个服务器失败而失败: %v", err)
	}

	var output struct {
		Servers []struct {
			ServerName string      `json:"server_name"`
			Success    bool        `json:"success"`
			Data       interface{} `json:"data"`
			Error      string      `json:"error"`
		} `json:"servers"`
	}
	if err := json.Unmarshal(data, &output); err != nil {
		t.Fatalf("解析输出失败: %v", err)
	}
	if len(output.Servers) != 2 {
		t.Fatalf("服务器数量不正确: got=%d data=%s", len(output.Servers), data)
	}
	if output.Servers[0].ServerName != "node-ok" || !output.Servers[0].Success || output.Servers[0].Data == nil {
		t.Fatalf("成功服务器结果不正确: %+v", output.Servers[0])
	}
	if output.Servers[1].ServerName != "node-fail" || output.Servers[1].Success || output.Servers[1].Error == "" {
		t.Fatalf("失败服务器结果不正确: %+v", output.Servers[1])
	}
}
