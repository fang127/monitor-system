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

// 定义错误变量
var (
	ErrInvalidCredentials = errors.New("用户名或密码错误")
	ErrUserDisabled       = errors.New("用户已被禁用")
	ErrForbidden          = errors.New("权限不足")
	ErrInvalidToken       = errors.New("无效或已过期的 token")
	ErrDuplicateUser      = errors.New("用户已存在")
)

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

// 定义 JWT Claims 结构体
type Claims struct {
	UserID   int64  `json:"user_id"`
	Username string `json:"username"`
	Role     Role   `json:"role"`
}
