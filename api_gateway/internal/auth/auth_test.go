package auth

import (
	"testing"
	"time"
)

func TestTokenManagerGeneratesAndParsesUserClaims(t *testing.T) {
	manager := NewTokenManager("test-secret", time.Hour)

	token, expiresAt, err := manager.Generate(User{
		ID:       42,
		Username: "alice",
		Role:     RoleAdmin,
	})
	if err != nil {
		t.Fatalf("Generate() error = %v", err)
	}
	if token == "" {
		t.Fatal("Generate() 返回空 token")
	}
	if !expiresAt.After(time.Now()) {
		t.Fatalf("expiresAt = %v, 应该晚于当前时间", expiresAt)
	}

	claims, err := manager.Parse(token)
	if err != nil {
		t.Fatalf("Parse() error = %v", err)
	}
	if claims.UserID != 42 || claims.Username != "alice" || claims.Role != RoleAdmin {
		t.Fatalf("claims = %+v, 用户信息不正确", claims)
	}
}

func TestTokenManagerRejectsExpiredToken(t *testing.T) {
	manager := NewTokenManager("test-secret", -time.Hour)
	token, _, err := manager.Generate(User{
		ID:       7,
		Username: "expired",
		Role:     RoleUser,
	})
	if err != nil {
		t.Fatalf("Generate() error = %v", err)
	}

	_, err = manager.Parse(token)
	if err == nil {
		t.Fatal("Parse() 应该拒绝过期 token")
	}
}
