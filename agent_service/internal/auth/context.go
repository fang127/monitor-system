package auth

import "context"

type bearerTokenKey struct{}
type claimsKey struct{}

// 带有 Bearer Token 的 Context
func ContextWithBearerToken(ctx context.Context, token string) context.Context {
	return context.WithValue(ctx, bearerTokenKey{}, token)
}

// 从 Context 中提取 Bearer Token
func BearerTokenFromContext(ctx context.Context) (string, bool) {
	token, ok := ctx.Value(bearerTokenKey{}).(string)
	return token, ok && token != ""
}

// ContextWithClaims 在 Context 中保存已解析的 JWT claims。
func ContextWithClaims(ctx context.Context, claims Claims) context.Context {
	return context.WithValue(ctx, claimsKey{}, claims)
}

// ClaimsFromContext 从 Context 中提取 JWT claims。
func ClaimsFromContext(ctx context.Context) (Claims, bool) {
	claims, ok := ctx.Value(claimsKey{}).(Claims)
	return claims, ok
}
