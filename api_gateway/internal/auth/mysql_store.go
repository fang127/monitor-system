package auth

import (
	"context"
	"errors"
	"strings"

	"golang.org/x/crypto/bcrypt"
	"gorm.io/gorm"
)

type MySQLUserStore struct {
	db *gorm.DB
}

// NewMySQLUserStore 创建一个新的 MySQL 用户存储实例
func NewMySQLUserStore(db *gorm.DB) *MySQLUserStore {
	return &MySQLUserStore{db: db}
}

// EnsureSchema 确保数据库中存在用户表
func (s *MySQLUserStore) EnsureSchema(ctx context.Context) error {
	return s.db.WithContext(ctx).AutoMigrate(&Tenant{}, &Team{}, &User{}, &UserTeamMembership{})
}

// BootstrapAdminIfEmpty 如果用户表为空，则创建一个管理员账户
// TODO：只在用户表创建不够，还需要在用户团队成员关系表中创建一个管理员的成员关系，关联到一个默认的租户和团队
func (s *MySQLUserStore) BootstrapAdminIfEmpty(ctx context.Context, username string, password string) error {
	var count int64
	if err := s.db.WithContext(ctx).Model(&User{}).Count(&count).Error; err != nil {
		return err
	}
	if count > 0 {
		return nil
	}
	username = strings.TrimSpace(username)
	if username == "" || password == "" {
		return errors.New("用户表为空时必须配置 ADMIN_USERNAME 和 ADMIN_PASSWORD")
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return err
	}
	// 事务创建管理员用户、默认租户和默认团队，并关联管理员用户到默认团队
	err = s.db.WithContext(ctx).Transaction(func(tx *gorm.DB) error {
		// 创建管理员用户
		adminUser := User{
			Username:     username,
			PasswordHash: string(hash),
			Role:         RoleAdmin,
			Status:       StatusActive,
		}
		if err := tx.Create(&adminUser).Error; err != nil {
			return err
		}
		// 创建默认租户
		defaultTenant := Tenant{
			Name:   "Default-Tenant-01",
			Status: StatusActive,
		}
		if err := tx.Create(&defaultTenant).Error; err != nil {
			return err
		}
		// 创建默认团队
		defaultTeam := Team{
			TenantID: defaultTenant.ID,
			Name:     "Default-Team-01",
			Status:   StatusActive,
		}
		if err := tx.Create(&defaultTeam).Error; err != nil {
			return err
		}
		// 关联管理员用户到默认团队
		membership := UserTeamMembership{
			UserID:     adminUser.ID,
			TenantID:   defaultTenant.ID,
			TeamID:     defaultTeam.ID,
			MemberRole: MemberRoleOwner,
			Status:     StatusActive,
		}
		if err := tx.Create(&membership).Error; err != nil {
			return err
		}
		return nil
	})

	return nil
}

// FindByUsername 根据用户名查找用户
func (s *MySQLUserStore) FindByUsername(username string) (User, error) {
	var user User
	err := s.db.WithContext(context.Background()).Where("username = ?", username).First(&user).Error
	if errors.Is(err, gorm.ErrRecordNotFound) {
		return User{}, ErrInvalidCredentials
	}
	return user, err
}

// ListActiveMemberships 查询用户可用的租户和团队（把这个用户“能访问哪些团队”查出来）
func (s *MySQLUserStore) ListActiveMemberships(userID int64) ([]TeamMembership, error) {
	var memberships []TeamMembership
	err := s.db.WithContext(context.Background()).
		Table("user_team_memberships AS m").
		Select("m.tenant_id, tenants.name AS tenant_name, m.team_id, teams.name AS team_name, m.member_role, m.status").
		Joins("JOIN tenants ON tenants.id = m.tenant_id").
		Joins("JOIN teams ON teams.id = m.team_id AND teams.tenant_id = m.tenant_id").
		Where("m.user_id = ? AND m.status = ? AND tenants.status = ? AND teams.status = ?", userID, StatusActive, StatusActive, StatusActive).
		Order("m.id ASC").
		Scan(&memberships).Error
	return memberships, err
}

// Create 创建一个新用户
// TODO：这里没有做事务，如果后续需要在创建用户的同时创建一些关联数据（比如默认的租户和团队），可能需要改成事务来处理。需要配合后期的用户注册功能来完善这个方法，目前先保持简单，后续如果有需要再改成事务来处理
func (s *MySQLUserStore) Create(username string, passwordHash string, role Role) (User, error) {
	user := User{
		Username:     username,
		PasswordHash: passwordHash,
		Role:         role,
		Status:       StatusActive,
	}
	err := s.db.WithContext(context.Background()).Create(&user).Error
	if err != nil {
		if errors.Is(err, gorm.ErrDuplicatedKey) {
			return User{}, ErrDuplicateUser
		}
		return User{}, err
	}
	return user, nil
}
