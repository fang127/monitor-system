package grpcclient

import "context"

type scopeContextKey struct{}

// Scope 表示监控查询需要携带的认证作用域。
type Scope struct {
	TenantID  string
	TeamID    string
	ClusterID string
}

// ContextWithScope 将认证作用域写入 context，供后续 gRPC 请求转发使用。
func ContextWithScope(ctx context.Context, scope Scope) context.Context {
	return context.WithValue(ctx, scopeContextKey{}, scope)
}

// ScopeFromContext 从 context 中读取认证作用域。
func ScopeFromContext(ctx context.Context) (Scope, bool) {
	scope, ok := ctx.Value(scopeContextKey{}).(Scope)
	return scope, ok && scope.TenantID != "" && scope.TeamID != ""
}
