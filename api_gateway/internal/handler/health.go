package handler

import (
	"net/http"

	"github.com/gin-gonic/gin"

	"monitor-system/api_gateway/internal/response"
)

type HealthHandler struct{}

func NewHealthHandler() *HealthHandler {
	return &HealthHandler{}
}

func (h *HealthHandler) Get(c *gin.Context) {
	response.OK(c, http.StatusOK, gin.H{
		"service": "api_gateway",
		"status":  "ok",
	})
}
