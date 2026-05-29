package auth

import (
	"errors"
	"testing"

	"github.com/DATA-DOG/go-sqlmock"
	"gorm.io/driver/mysql"
	"gorm.io/gorm"
)

func newMockGormDB(t *testing.T) (*gorm.DB, sqlmock.Sqlmock, func()) {
	t.Helper()
	sqlDB, mock, err := sqlmock.New(sqlmock.QueryMatcherOption(sqlmock.QueryMatcherRegexp))
	if err != nil {
		t.Fatalf("创建 sqlmock 失败: %v", err)
	}
	db, err := gorm.Open(mysql.New(mysql.Config{
		Conn:                      sqlDB,
		SkipInitializeWithVersion: true,
	}), &gorm.Config{TranslateError: true})
	if err != nil {
		t.Fatalf("创建 gorm db 失败: %v", err)
	}
	return db, mock, func() {
		_ = sqlDB.Close()
	}
}

func TestGormUserStoreFindByUsername(t *testing.T) {
	db, mock, cleanup := newMockGormDB(t)
	defer cleanup()

	rows := sqlmock.NewRows([]string{"id", "username", "password_hash", "role", "status"}).
		AddRow(12, "alice", "hash", "admin", "active")
	mock.ExpectQuery("SELECT \\* FROM `users` WHERE username = \\? ORDER BY `users`.`id` LIMIT \\?").
		WithArgs("alice", 1).
		WillReturnRows(rows)

	store := NewMySQLUserStore(db)
	user, err := store.FindByUsername("alice")
	if err != nil {
		t.Fatalf("FindByUsername() error = %v", err)
	}
	if user.ID != 12 || user.Username != "alice" || user.Role != RoleAdmin || user.Status != StatusActive {
		t.Fatalf("user = %+v, 不符合预期", user)
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("SQL 预期未满足: %v", err)
	}
}

func TestGormUserStoreFindByUsernameMapsNotFound(t *testing.T) {
	db, mock, cleanup := newMockGormDB(t)
	defer cleanup()

	rows := sqlmock.NewRows([]string{"id", "username", "password_hash", "role", "status"})
	mock.ExpectQuery("SELECT \\* FROM `users` WHERE username = \\? ORDER BY `users`.`id` LIMIT \\?").
		WithArgs("missing", 1).
		WillReturnRows(rows)

	store := NewMySQLUserStore(db)
	_, err := store.FindByUsername("missing")
	if !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("FindByUsername() error = %v, want ErrInvalidCredentials", err)
	}
	if err := mock.ExpectationsWereMet(); err != nil {
		t.Fatalf("SQL 预期未满足: %v", err)
	}
}
