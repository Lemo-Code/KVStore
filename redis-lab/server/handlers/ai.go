package handlers

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"

	"redis-lab/server/config"
	"redis-lab/server/models"
	"redis-lab/server/services"
)

type AIHandler struct {
	aiService *services.AIService
}

func NewAIHandler(cfg *config.Config) *AIHandler {
	return &AIHandler{
		aiService: services.NewAIService(cfg),
	}
}

func (h *AIHandler) Chat(c *gin.Context) {
	userID := c.GetUint("user_id")

	var req models.AIChatRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Build context if connection_id provided
	var aiCtx services.AIChatContext
	if req.ConnectionID != nil {
		// Get recent commands from this session? For now, just note the connection
		aiCtx.ConnectedServer = fmt.Sprintf("ledis:%d", *req.ConnectionID)
	}

	// Set SSE headers
	c.Header("Content-Type", "text/event-stream")
	c.Header("Cache-Control", "no-cache")
	c.Header("Connection", "keep-alive")
	c.Header("Access-Control-Allow-Origin", "*")
	c.Header("X-Accel-Buffering", "no")

	tokenChan, errChan, err := h.aiService.Chat(userID, req, aiCtx)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	flusher, ok := c.Writer.(http.Flusher)
	if !ok {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "流式响应不支持"})
		return
	}

	// Stream tokens
	c.Stream(func(w io.Writer) bool {
		select {
		case token, ok := <-tokenChan:
			if !ok {
				// Channel closed, done
				fmt.Fprintf(w, "event: done\ndata: {}\n\n")
				flusher.Flush()
				return false
			}
			data, _ := json.Marshal(map[string]string{"token": token})
			fmt.Fprintf(w, "data: %s\n\n", string(data))
			flusher.Flush()
			return true

		case err, ok := <-errChan:
			if ok && err != nil {
				data, _ := json.Marshal(map[string]string{"error": err.Error()})
				fmt.Fprintf(w, "event: error\ndata: %s\n\n", string(data))
				flusher.Flush()
			}
			return false
		}
	})
}

func (h *AIHandler) GetConversations(c *gin.Context) {
	userID := c.GetUint("user_id")

	conversations, err := services.GetConversations(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "获取会话列表失败"})
		return
	}

	if conversations == nil {
		conversations = []models.AIConversation{}
	}

	c.JSON(http.StatusOK, gin.H{"conversations": conversations})
}

func (h *AIHandler) GetConversation(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的会话 ID"})
		return
	}

	messages, err := services.GetConversationMessages(uint(id), userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	if messages == nil {
		messages = []models.AIMessage{}
	}

	c.JSON(http.StatusOK, gin.H{"messages": messages})
}

func (h *AIHandler) DeleteConversation(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的会话 ID"})
		return
	}

	if err := services.DeleteConversation(uint(id), userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "会话已删除"})
}
