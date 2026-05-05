package handler

import (
	"net/http"

	"github.com/gin-gonic/gin"

	"monitor-system/api_gateway/internal/response"
)

type VersionHandler struct {
	version string
}

func NewVersionHandler(version string) *VersionHandler {
	return &VersionHandler{version: version}
}

func (h *VersionHandler) Get(c *gin.Context) {
	response.OK(c, http.StatusOK, gin.H{
		"service": "api_gateway",
		"version": h.version,
	})
}
