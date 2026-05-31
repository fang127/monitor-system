package auth

import (
	"errors"
	"time"
)

// 定义用户角色
type Role string

const (
	RoleAdmin Role = "admin"
	RoleUser  Role = "user"
)

// 定义用户状态
type Status string

const (
	StatusActive   Status = "active"
	StatusDisabled Status = "disabled"
)

// MemberRole 定义用户在团队内的成员角色
type MemberRole string

const (
	MemberRoleOwner  MemberRole = "owner"  // 团队所有者，拥有最高权限
	MemberRoleAdmin  MemberRole = "admin"  // 团队管理员，拥有管理权限但不如所有者
	MemberRoleMember MemberRole = "member" // 团队成员，拥有基本访问权限
	MemberRoleViewer MemberRole = "viewer" // 团队观察者，拥有只读权限
)

// 定义错误变量
var (
	ErrInvalidCredentials = errors.New("用户名或密码错误")
	ErrUserDisabled       = errors.New("用户已被禁用")
	ErrForbidden          = errors.New("权限不足")
	ErrInvalidToken       = errors.New("无效或已过期的 token")
	ErrDuplicateUser      = errors.New("用户已存在")
	ErrNoActiveTeam       = errors.New("用户没有可用团队")
	ErrTeamRequired       = errors.New("请选择要访问的团队")
)

// Tenant 定义租户结构体
type Tenant struct {
	ID        string    `json:"id" gorm:"column:id;size:128;primaryKey"`
	Name      string    `json:"name" gorm:"column:name;size:255;not null"`
	Status    Status    `json:"status" gorm:"column:status;type:enum('active','disabled');not null;default:active"`
	CreatedAt time.Time `json:"-" gorm:"column:created_at"`
	UpdatedAt time.Time `json:"-" gorm:"column:updated_at"`
}

func (Tenant) TableName() string {
	return "tenants"
}

// Team 定义团队结构体
type Team struct {
	ID        string    `json:"id" gorm:"column:id;size:128;primaryKey"`
	TenantID  string    `json:"tenant_id" gorm:"column:tenant_id;size:128;not null;index:idx_teams_tenant_status,priority:1;uniqueIndex:uk_teams_tenant_name,priority:1"`
	Name      string    `json:"name" gorm:"column:name;size:255;not null;uniqueIndex:uk_teams_tenant_name,priority:2"`
	Status    Status    `json:"status" gorm:"column:status;type:enum('active','disabled');not null;default:active;index:idx_teams_tenant_status,priority:2"`
	CreatedAt time.Time `json:"-" gorm:"column:created_at"`
	UpdatedAt time.Time `json:"-" gorm:"column:updated_at"`
}

func (Team) TableName() string {
	return "teams"
}

// 定义用户结构体
type User struct {
	ID           int64     `json:"id" gorm:"column:id;primaryKey;autoIncrement"`
	Username     string    `json:"username" gorm:"column:username;size:64;uniqueIndex;not null"`
	PasswordHash string    `json:"-" gorm:"column:password_hash;size:255;not null"`
	Role         Role      `json:"role" gorm:"column:role;type:enum('admin','user');not null;default:user"`
	Status       Status    `json:"status" gorm:"column:status;type:enum('active','disabled');not null;default:active"`
	CreatedAt    time.Time `json:"-" gorm:"column:created_at"`
	UpdatedAt    time.Time `json:"-" gorm:"column:updated_at"`
}

func (User) TableName() string {
	return "users"
}

// UserTeamMembership 定义用户和团队之间的成员关系
type UserTeamMembership struct {
	ID         int64      `json:"id" gorm:"column:id;primaryKey;autoIncrement"`
	UserID     int64      `json:"user_id" gorm:"column:user_id;not null;uniqueIndex:uk_user_team_membership,priority:1;index:idx_membership_user_status,priority:1"`
	TenantID   string     `json:"tenant_id" gorm:"column:tenant_id;size:128;not null;uniqueIndex:uk_user_team_membership,priority:2;index:idx_membership_scope_status,priority:1"`
	TeamID     string     `json:"team_id" gorm:"column:team_id;size:128;not null;uniqueIndex:uk_user_team_membership,priority:3;index:idx_membership_scope_status,priority:2"`
	MemberRole MemberRole `json:"member_role" gorm:"column:member_role;type:enum('owner','admin','member','viewer');not null;default:member"`
	Status     Status     `json:"status" gorm:"column:status;type:enum('active','disabled');not null;default:active;index:idx_membership_scope_status,priority:3;index:idx_membership_user_status,priority:2"`
	CreatedAt  time.Time  `json:"-" gorm:"column:created_at"`
	UpdatedAt  time.Time  `json:"-" gorm:"column:updated_at"`
}

func (UserTeamMembership) TableName() string {
	return "user_team_memberships"
}

// TeamMembership 是返回给业务层和前端的当前可访问团队信息
type TeamMembership struct {
	TenantID   string     `json:"tenant_id"`
	TenantName string     `json:"tenant_name,omitempty"`
	TeamID     string     `json:"team_id"`
	TeamName   string     `json:"team_name,omitempty"`
	MemberRole MemberRole `json:"member_role"`
	Status     Status     `json:"status,omitempty"`
}

// 定义 JWT Claims 结构体
type Claims struct {
	UserID   int64  `json:"user_id"`
	Username string `json:"username"`
	Role     Role   `json:"role"`
	TenantID string `json:"tenant_id"`
	TeamID   string `json:"team_id"`
}
