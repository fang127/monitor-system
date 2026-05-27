package gateway

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"strings"
	"testing"
)

func TestClientUnwrapsEnvelope(t *testing.T) {
	client := NewClient("http://gateway.test", &http.Client{Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
		if r.URL.Path != "/api/servers/latest" {
			t.Fatalf("path = %s", r.URL.Path)
		}
		return response(`{"code":0,"message":"ok","data":{"servers":[{"server_name":"s1","score":91}],"cluster_stats":{"total_servers":1}}}`), nil
	})})
	raw, err := client.Latest(context.Background())
	if err != nil {
		t.Fatalf("Latest returned error: %v", err)
	}

	var payload struct {
		Servers []ServerSummary `json:"servers"`
	}
	if err := json.Unmarshal(raw, &payload); err != nil {
		t.Fatal(err)
	}
	if len(payload.Servers) != 1 || payload.Servers[0].ServerName != "s1" {
		t.Fatalf("unexpected payload: %s", raw)
	}
}

func TestClientPreservesGatewayErrors(t *testing.T) {
	client := NewClient("http://gateway.test", &http.Client{Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
		return response(`{"code":503,"message":"manager unavailable"}`), nil
	})})
	_, err := client.Latest(context.Background())
	if err == nil || !strings.Contains(err.Error(), "manager unavailable") {
		t.Fatalf("err = %v, want gateway message", err)
	}
}

func TestAnomaliesForEscapesServerName(t *testing.T) {
	client := NewClient("http://gateway.test", &http.Client{Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
		if !strings.Contains(r.URL.EscapedPath(), "server%2Fone") {
			t.Fatalf("path was not escaped: %s", r.URL.EscapedPath())
		}
		return response(`{"code":0,"message":"ok","data":{"anomalies":[]}}`), nil
	})})
	if _, err := client.Anomalies(context.Background(), "server/one", Query{}); err != nil {
		t.Fatal(err)
	}
}

type roundTripFunc func(*http.Request) (*http.Response, error)

func (f roundTripFunc) RoundTrip(r *http.Request) (*http.Response, error) {
	return f(r)
}

func response(body string) *http.Response {
	return &http.Response{
		StatusCode: http.StatusOK,
		Body:       io.NopCloser(strings.NewReader(body)),
		Header:     http.Header{"Content-Type": []string{"application/json"}},
	}
}
