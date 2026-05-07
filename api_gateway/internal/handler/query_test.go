package handler

import (
	"net/http"
	"testing"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

func TestGRPCHTTPStatus(t *testing.T) {
	tests := []struct {
		name string
		code codes.Code
		want int
	}{
		{name: "invalid argument", code: codes.InvalidArgument, want: http.StatusBadRequest},
		{name: "not found", code: codes.NotFound, want: http.StatusNotFound},
		{name: "deadline exceeded", code: codes.DeadlineExceeded, want: http.StatusGatewayTimeout},
		{name: "unavailable", code: codes.Unavailable, want: http.StatusBadGateway},
		{name: "resource exhausted", code: codes.ResourceExhausted, want: http.StatusTooManyRequests},
		{name: "internal", code: codes.Internal, want: http.StatusInternalServerError},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := status.Error(tt.code, tt.name)
			if got := grpcHTTPStatus(err); got != tt.want {
				t.Fatalf("grpcHTTPStatus(%v) = %d, want %d", tt.code, got, tt.want)
			}
		})
	}
}
