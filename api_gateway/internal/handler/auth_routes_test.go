package handler

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"monitor-system/api_gateway/internal/auth"
	"monitor-system/api_gateway/internal/config"

	"golang.org/x/crypto/bcrypt"
)

type memoryUserStore struct {
	users map[string]auth.User
}

func newMemoryUserStore() *memoryUserStore {
	hash, err := bcrypt.GenerateFromPassword([]byte("secret"), bcrypt.DefaultCost)
	if err != nil {
		panic(err)
	}
	return &memoryUserStore{
		users: map[string]auth.User{
			"admin": {
				ID:           1,
				Username:     "admin",
				PasswordHash: string(hash),
				Role:         auth.RoleAdmin,
				Status:       auth.StatusActive,
			},
			"viewer": {
				ID:           2,
				Username:     "viewer",
				PasswordHash: string(hash),
				Role:         auth.RoleUser,
				Status:       auth.StatusActive,
			},
		},
	}
}

func (s *memoryUserStore) FindByUsername(username string) (auth.User, error) {
	user, ok := s.users[username]
	if !ok {
		return auth.User{}, auth.ErrInvalidCredentials
	}
	return user, nil
}

func (s *memoryUserStore) Create(username string, password string, role auth.Role) (auth.User, error) {
	user := auth.User{
		ID:           int64(len(s.users) + 1),
		Username:     username,
		PasswordHash: password,
		Role:         role,
		Status:       auth.StatusActive,
	}
	s.users[username] = user
	return user, nil
}

func TestRouterProtectsMonitorAPIsAndKeepsPublicEndpoints(t *testing.T) {
	router := NewRouter(config.Config{
		Mode:         "test",
		Version:      "v-test",
		JWTSecret:    "test-secret",
		JWTAccessTTL: time.Hour,
	}, nil, RouterOptions{
		UserStore: newMemoryUserStore(),
	})

	protected := httptest.NewRecorder()
	router.ServeHTTP(protected, httptest.NewRequest(http.MethodGet, "/api/servers/latest", nil))
	if protected.Code != http.StatusUnauthorized {
		t.Fatalf("未携带 token 访问监控接口状态码 = %d, want %d", protected.Code, http.StatusUnauthorized)
	}

	version := httptest.NewRecorder()
	router.ServeHTTP(version, httptest.NewRequest(http.MethodGet, "/api/version", nil))
	if version.Code != http.StatusOK {
		t.Fatalf("公开版本接口状态码 = %d, want %d", version.Code, http.StatusOK)
	}
}

func TestLoginReturnsBearerTokenAndMeUsesClaims(t *testing.T) {
	router := NewRouter(config.Config{
		Mode:         "test",
		Version:      "v-test",
		JWTSecret:    "test-secret",
		JWTAccessTTL: time.Hour,
	}, nil, RouterOptions{
		UserStore: newMemoryUserStore(),
	})

	body := bytes.NewBufferString(`{"username":"admin","password":"secret"}`)
	login := httptest.NewRecorder()
	router.ServeHTTP(login, httptest.NewRequest(http.MethodPost, "/api/auth/login", body))
	if login.Code != http.StatusOK {
		t.Fatalf("登录状态码 = %d, body = %s", login.Code, login.Body.String())
	}
	var loginPayload struct {
		Code int `json:"code"`
		Data struct {
			AccessToken string `json:"access_token"`
			User        struct {
				Username string `json:"username"`
				Role     string `json:"role"`
			} `json:"user"`
		} `json:"data"`
	}
	if err := json.Unmarshal(login.Body.Bytes(), &loginPayload); err != nil {
		t.Fatalf("解析登录响应失败: %v", err)
	}
	if loginPayload.Data.AccessToken == "" {
		t.Fatal("登录响应缺少 access_token")
	}
	if loginPayload.Data.User.Username != "admin" || loginPayload.Data.User.Role != string(auth.RoleAdmin) {
		t.Fatalf("登录用户信息 = %+v, 不正确", loginPayload.Data.User)
	}

	meReq := httptest.NewRequest(http.MethodGet, "/api/auth/me", nil)
	meReq.Header.Set("Authorization", "Bearer "+loginPayload.Data.AccessToken)
	me := httptest.NewRecorder()
	router.ServeHTTP(me, meReq)
	if me.Code != http.StatusOK {
		t.Fatalf("/api/auth/me 状态码 = %d, body = %s", me.Code, me.Body.String())
	}
}

func TestCreateUserRequiresAdminRole(t *testing.T) {
	store := newMemoryUserStore()
	router := NewRouter(config.Config{
		Mode:         "test",
		Version:      "v-test",
		JWTSecret:    "test-secret",
		JWTAccessTTL: time.Hour,
	}, nil, RouterOptions{
		UserStore: store,
	})
	tokenManager := auth.NewTokenManager("test-secret", time.Hour)
	userToken, _, err := tokenManager.Generate(store.users["viewer"])
	if err != nil {
		t.Fatalf("生成普通用户 token 失败: %v", err)
	}
	adminToken, _, err := tokenManager.Generate(store.users["admin"])
	if err != nil {
		t.Fatalf("生成管理员 token 失败: %v", err)
	}

	reqBody := bytes.NewBufferString(`{"username":"ops","password":"secret","role":"user"}`)
	userReq := httptest.NewRequest(http.MethodPost, "/api/users", reqBody)
	userReq.Header.Set("Authorization", "Bearer "+userToken)
	userResp := httptest.NewRecorder()
	router.ServeHTTP(userResp, userReq)
	if userResp.Code != http.StatusForbidden {
		t.Fatalf("普通用户创建账号状态码 = %d, want %d", userResp.Code, http.StatusForbidden)
	}

	adminReq := httptest.NewRequest(http.MethodPost, "/api/users", bytes.NewBufferString(`{"username":"ops","password":"secret","role":"user"}`))
	adminReq.Header.Set("Authorization", "Bearer "+adminToken)
	adminResp := httptest.NewRecorder()
	router.ServeHTTP(adminResp, adminReq)
	if adminResp.Code != http.StatusCreated {
		t.Fatalf("管理员创建账号状态码 = %d, body = %s", adminResp.Code, adminResp.Body.String())
	}
}
