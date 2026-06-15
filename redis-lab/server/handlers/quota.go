package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"

	"redis-lab/server/services"
)

type QuotaHandler struct {
	quotaService *services.QuotaService
}

func NewQuotaHandler(qs *services.QuotaService) *QuotaHandler {
	return &QuotaHandler{quotaService: qs}
}

// GetQuotaStatus returns the current resource usage for the authenticated user.
func (h *QuotaHandler) GetStatus(c *gin.Context) {
	userID := c.GetUint("user_id")

	status, err := h.quotaService.GetQuotaStatus(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"quota": status})
}
