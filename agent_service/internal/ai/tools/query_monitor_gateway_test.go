package tools

import (
	"context"
	"net/http"
	"net/url"
	"testing"

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
