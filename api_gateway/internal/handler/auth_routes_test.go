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
	users       map[string]auth.User
	memberships map[int64][]auth.TeamMembership
}

func newMemoryUserStore() *memoryUserStore {
	hash, err := bcrypt.GenerateFromPassword([]byte("secret"), bcrypt.DefaultCost)
	if err != nil {
		panic(err)
	}
	store := &memoryUserStore{
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
	store.memberships = map[int64][]auth.TeamMembership{
		1: {
			{TenantID: "tenant-a", TenantName: "租户 A", TeamID: "ops", TeamName: "运维团队", MemberRole: auth.MemberRoleAdmin, Status: auth.StatusActive},
			{TenantID: "tenant-a", TenantName: "租户 A", TeamID: "db", TeamName: "数据库团队", MemberRole: auth.MemberRoleAdmin, Status: auth.StatusActive},
		},
		2: {
			{TenantID: "tenant-a", TenantName: "租户 A", TeamID: "ops", TeamName: "运维团队", MemberRole: auth.MemberRoleViewer, Status: auth.StatusActive},
		},
	}
	return store
}

func (s *memoryUserStore) FindByUsername(username string) (auth.User, error) {
	user, ok := s.users[username]
	if !ok {
		return auth.User{}, auth.ErrInvalidCredentials
	}
	return user, nil
}

func (s *memoryUserStore) ListActiveMemberships(userID int64) ([]auth.TeamMembership, error) {
	return append([]auth.TeamMembership(nil), s.memberships[userID]...), nil
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
	s.memberships[user.ID] = []auth.TeamMembership{
		{TenantID: "tenant-a", TenantName: "租户 A", TeamID: "ops", TeamName: "运维团队", MemberRole: auth.MemberRoleMember, Status: auth.StatusActive},
	}
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

func TestMonitorAPIsRejectMismatchedScopeQuery(t *testing.T) {
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
	token, _, err := tokenManager.Generate(store.users["viewer"], store.memberships[2][0])
	if err != nil {
		t.Fatalf("生成普通用户 token 失败: %v", err)
	}

	req := httptest.NewRequest(http.MethodGet, "/api/servers/latest?tenant_id=tenant-b", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	resp := httptest.NewRecorder()
	router.ServeHTTP(resp, req)
	if resp.Code != http.StatusForbidden {
		t.Fatalf("越权租户查询状态码 = %d, body = %s", resp.Code, resp.Body.String())
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

	body := bytes.NewBufferString(`{"username":"admin","password":"secret","tenant_id":"tenant-a","team_id":"ops"}`)
	login := httptest.NewRecorder()
	router.ServeHTTP(login, httptest.NewRequest(http.MethodPost, "/api/auth/login", body))
	if login.Code != http.StatusOK {
		t.Fatalf("登录状态码 = %d, body = %s", login.Code, login.Body.String())
	}
	var loginPayload struct {
		Code int `json:"code"`
		Data struct {
			AccessToken  string `json:"access_token"`
			CurrentScope struct {
				TenantID string `json:"tenant_id"`
				TeamID   string `json:"team_id"`
			} `json:"current_scope"`
			User struct {
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
	if loginPayload.Data.CurrentScope.TenantID != "tenant-a" || loginPayload.Data.CurrentScope.TeamID != "ops" {
		t.Fatalf("登录作用域 = %+v, 不正确", loginPayload.Data.CurrentScope)
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
	var mePayload struct {
		Data struct {
			CurrentScope struct {
				TenantID string `json:"tenant_id"`
				TeamID   string `json:"team_id"`
			} `json:"current_scope"`
			Teams []struct {
				TeamID string `json:"team_id"`
			} `json:"teams"`
		} `json:"data"`
	}
	if err := json.Unmarshal(me.Body.Bytes(), &mePayload); err != nil {
		t.Fatalf("解析 /api/auth/me 响应失败: %v", err)
	}
	if mePayload.Data.CurrentScope.TeamID != "ops" || len(mePayload.Data.Teams) != 2 {
		t.Fatalf("/api/auth/me 作用域 = %+v, teams=%+v", mePayload.Data.CurrentScope, mePayload.Data.Teams)
	}
}

func TestSwitchTeamIssuesScopedToken(t *testing.T) {
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
	adminToken, _, err := tokenManager.Generate(store.users["admin"], store.memberships[1][0])
	if err != nil {
		t.Fatalf("生成管理员 token 失败: %v", err)
	}

	req := httptest.NewRequest(http.MethodPost, "/api/auth/switch-team", bytes.NewBufferString(`{"tenant_id":"tenant-a","team_id":"db"}`))
	req.Header.Set("Authorization", "Bearer "+adminToken)
	resp := httptest.NewRecorder()
	router.ServeHTTP(resp, req)
	if resp.Code != http.StatusOK {
		t.Fatalf("切换团队状态码 = %d, body = %s", resp.Code, resp.Body.String())
	}
	var payload struct {
		Data struct {
			AccessToken  string `json:"access_token"`
			CurrentScope struct {
				TenantID string `json:"tenant_id"`
				TeamID   string `json:"team_id"`
			} `json:"current_scope"`
		} `json:"data"`
	}
	if err := json.Unmarshal(resp.Body.Bytes(), &payload); err != nil {
		t.Fatalf("解析切换团队响应失败: %v", err)
	}
	if payload.Data.CurrentScope.TeamID != "db" || payload.Data.AccessToken == "" {
		t.Fatalf("切换团队响应不正确: %+v", payload.Data.CurrentScope)
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
	userToken, _, err := tokenManager.Generate(store.users["viewer"], store.memberships[2][0])
	if err != nil {
		t.Fatalf("生成普通用户 token 失败: %v", err)
	}
	adminToken, _, err := tokenManager.Generate(store.users["admin"], store.memberships[1][0])
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
