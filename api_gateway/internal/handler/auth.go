package handler

import (
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"

	"monitor-system/api_gateway/internal/auth"
	"monitor-system/api_gateway/internal/response"
)

type AuthHandler struct {
	service *auth.Service
}

func NewAuthHandler(service *auth.Service) *AuthHandler {
	return &AuthHandler{service: service}
}

// 登录请求体
type loginRequest struct {
	Username string `json:"username"`
	Password string `json:"password"`
	TenantID string `json:"tenant_id"`
	TeamID   string `json:"team_id"`
}

// 切换团队请求体
type switchTeamRequest struct {
	TenantID string `json:"tenant_id"`
	TeamID   string `json:"team_id"`
}

// 创建用户请求体
type createUserRequest struct {
	Username string    `json:"username"`
	Password string    `json:"password"`
	Role     auth.Role `json:"role"`
}

// Login 处理用户登录请求
func (h *AuthHandler) Login(c *gin.Context) {
	var req loginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		response.Error(c, http.StatusBadRequest, "请求体格式错误")
		return
	}
	result, err := h.service.Login(req.Username, req.Password, req.TenantID, req.TeamID)
	if err != nil {
		status := http.StatusInternalServerError
		if errors.Is(err, auth.ErrInvalidCredentials) || errors.Is(err, auth.ErrUserDisabled) {
			status = http.StatusUnauthorized
		}
		if errors.Is(err, auth.ErrNoActiveTeam) || errors.Is(err, auth.ErrTeamRequired) || errors.Is(err, auth.ErrForbidden) {
			status = http.StatusForbidden
		}
		response.Error(c, status, err.Error())
		return
	}
	response.OK(c, http.StatusOK, result)
}

// Me 返回当前登录用户的信息
func (h *AuthHandler) Me(c *gin.Context) {
	claims, ok := auth.CurrentClaims(c)
	if !ok {
		response.Error(c, http.StatusUnauthorized, "未登录或登录已过期")
		return
	}
	currentScope, teams, err := h.service.CurrentContext(claims)
	if err != nil {
		response.Error(c, http.StatusForbidden, err.Error())
		return
	}
	response.OK(c, http.StatusOK, gin.H{
		"id":            claims.UserID,
		"username":      claims.Username,
		"role":          claims.Role,
		"tenant_id":     claims.TenantID,
		"team_id":       claims.TeamID,
		"current_scope": currentScope,
		"teams":         teams,
	})
}

// SwitchTeam 切换当前访问团队
func (h *AuthHandler) SwitchTeam(c *gin.Context) {
	claims, ok := auth.CurrentClaims(c)
	if !ok {
		response.Error(c, http.StatusUnauthorized, "未登录或登录已过期")
		return
	}
	var req switchTeamRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		response.Error(c, http.StatusBadRequest, "请求体格式错误")
		return
	}
	result, err := h.service.SwitchTeam(claims, req.TenantID, req.TeamID)
	if err != nil {
		status := http.StatusForbidden
		if !errors.Is(err, auth.ErrForbidden) && !errors.Is(err, auth.ErrNoActiveTeam) && !errors.Is(err, auth.ErrTeamRequired) {
			status = http.StatusInternalServerError
		}
		response.Error(c, status, err.Error())
		return
	}
	response.OK(c, http.StatusOK, result)
}

// CreateUser 处理创建新用户的请求
func (h *AuthHandler) CreateUser(c *gin.Context) {
	var req createUserRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		response.Error(c, http.StatusBadRequest, "请求体格式错误")
		return
	}
	user, err := h.service.CreateUser(req.Username, req.Password, req.Role)
	if err != nil {
		status := http.StatusBadRequest
		if errors.Is(err, auth.ErrDuplicateUser) {
			status = http.StatusConflict
		}
		response.Error(c, status, err.Error())
		return
	}
	response.OK(c, http.StatusCreated, user)
}
