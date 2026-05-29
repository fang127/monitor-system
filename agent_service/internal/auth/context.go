package auth

import "context"

type bearerTokenKey struct{}

// 带有 Bearer Token 的 Context
func ContextWithBearerToken(ctx context.Context, token string) context.Context {
	return context.WithValue(ctx, bearerTokenKey{}, token)
}

// 从 Context 中提取 Bearer Token
func BearerTokenFromContext(ctx context.Context) (string, bool) {
	token, ok := ctx.Value(bearerTokenKey{}).(string)
	return token, ok && token != ""
}
