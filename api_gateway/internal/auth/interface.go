package auth

type UserStore interface {
	FindByUsername(username string) (User, error)                         // 根据用户名查找用户
	ListActiveMemberships(userID int64) ([]TeamMembership, error)         // 列出用户的所有活跃团队成员关系
	Create(username string, passwordHash string, role Role) (User, error) // 创建新用户
}
