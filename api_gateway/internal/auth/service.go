package auth

import (
	"errors"
	"strings"
	"time"

	"golang.org/x/crypto/bcrypt"
)

// 给前端返回的用户信息，不包含敏感字段
type LoginResult struct {
	AccessToken  string           `json:"access_token"`
	ExpiresAt    time.Time        `json:"expires_at"`
	User         User             `json:"user"`
	CurrentScope TeamMembership   `json:"current_scope"`
	Teams        []TeamMembership `json:"teams"`
}

type Service struct {
	store        UserStore
	tokenManager *TokenManager
}

// NewService 创建一个新的认证服务实例
func NewService(store UserStore, tokenManager *TokenManager) *Service {
	return &Service{
		store:        store,
		tokenManager: tokenManager,
	}
}

// Login 验证用户凭据并生成访问令牌
func (s *Service) Login(username string, password string, tenantID string, teamID string) (LoginResult, error) {
	if s == nil || s.store == nil || s.tokenManager == nil {
		return LoginResult{}, errors.New("认证服务未配置")
	}
	username = strings.TrimSpace(username)
	if username == "" || password == "" {
		return LoginResult{}, ErrInvalidCredentials
	}
	// 从存储中查找用户
	user, err := s.store.FindByUsername(username)
	if err != nil {
		return LoginResult{}, ErrInvalidCredentials
	}
	if user.Status != StatusActive {
		return LoginResult{}, ErrUserDisabled
	}
	// bcrypt.CompareHashAndPassword 返回 nil 表示密码匹配，非 nil 表示不匹配或发生错误
	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(password)); err != nil {
		return LoginResult{}, ErrInvalidCredentials
	}
	// 获取用户的团队成员关系并选择当前访问的团队范围
	memberships, err := s.store.ListActiveMemberships(user.ID)
	if err != nil {
		return LoginResult{}, err
	}
	scope, err := selectMembership(memberships, tenantID, teamID)
	if err != nil {
		return LoginResult{}, err
	}
	token, expiresAt, err := s.tokenManager.Generate(user, scope)
	if err != nil {
		return LoginResult{}, err
	}
	return LoginResult{
		AccessToken:  token,
		ExpiresAt:    expiresAt,
		User:         sanitizeUser(user),
		CurrentScope: scope,
		Teams:        memberships,
	}, nil
}

// SwitchTeam 切换当前访问团队并重新签发带作用域的访问令牌。
func (s *Service) SwitchTeam(claims Claims, tenantID string, teamID string) (LoginResult, error) {
	if s == nil || s.store == nil || s.tokenManager == nil {
		return LoginResult{}, errors.New("认证服务未配置")
	}
	memberships, err := s.store.ListActiveMemberships(claims.UserID)
	if err != nil {
		return LoginResult{}, err
	}
	scope, err := selectMembership(memberships, tenantID, teamID)
	if err != nil {
		return LoginResult{}, err
	}
	user := User{
		ID:       claims.UserID,
		Username: claims.Username,
		Role:     claims.Role,
		Status:   StatusActive,
	}
	token, expiresAt, err := s.tokenManager.Generate(user, scope)
	if err != nil {
		return LoginResult{}, err
	}
	return LoginResult{
		AccessToken:  token,
		ExpiresAt:    expiresAt,
		User:         sanitizeUser(user),
		CurrentScope: scope,
		Teams:        memberships,
	}, nil
}

// CurrentContext 返回当前令牌对应的团队上下文和用户可切换团队列表。
func (s *Service) CurrentContext(claims Claims) (TeamMembership, []TeamMembership, error) {
	if s == nil || s.store == nil {
		return TeamMembership{}, nil, errors.New("认证服务未配置")
	}
	memberships, err := s.store.ListActiveMemberships(claims.UserID)
	if err != nil {
		return TeamMembership{}, nil, err
	}
	scope, err := selectMembership(memberships, claims.TenantID, claims.TeamID)
	if err != nil {
		return TeamMembership{}, nil, err
	}
	return scope, memberships, nil
}

// CreateUser 创建一个新用户，返回创建的用户信息（不包含密码哈希）
func (s *Service) CreateUser(username string, password string, role Role) (User, error) {
	if s == nil || s.store == nil {
		return User{}, errors.New("认证服务未配置")
	}
	username = strings.TrimSpace(username)
	if username == "" || password == "" {
		return User{}, errors.New("用户名和密码不能为空")
	}
	if role == "" {
		role = RoleUser
	}
	if role != RoleAdmin && role != RoleUser {
		return User{}, errors.New("角色必须是 admin 或 user")
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return User{}, err
	}
	user, err := s.store.Create(username, string(hash), role)
	if err != nil {
		return User{}, err
	}
	return sanitizeUser(user), nil
}

// sanitizeUser 从用户信息中移除敏感字段（如密码哈希），以便安全地返回给前端
func sanitizeUser(user User) User {
	user.PasswordHash = ""
	return user
}

// selectMembership 根据提供的租户ID和团队ID从用户的团队成员关系列表中选择一个匹配的成员关系
func selectMembership(memberships []TeamMembership, tenantID string, teamID string) (TeamMembership, error) {
	tenantID = strings.TrimSpace(tenantID)
	teamID = strings.TrimSpace(teamID)
	if len(memberships) == 0 {
		return TeamMembership{}, ErrNoActiveTeam
	}
	if tenantID == "" && teamID == "" {
		if len(memberships) == 1 {
			return memberships[0], nil
		}
		return TeamMembership{}, ErrTeamRequired
	}
	if tenantID == "" || teamID == "" {
		return TeamMembership{}, ErrTeamRequired
	}
	for _, membership := range memberships {
		if membership.TenantID == tenantID && membership.TeamID == teamID {
			return membership, nil
		}
	}
	return TeamMembership{}, ErrForbidden
}
