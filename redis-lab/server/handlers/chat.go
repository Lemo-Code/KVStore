package handlers

import (
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"

	"redis-lab/server/database"
	"redis-lab/server/models"
	"redis-lab/server/services"
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return true // Allow all origins in development
	},
}

type ChatHandler struct {
	hub *services.ChatHub
}

func NewChatHandler(hub *services.ChatHub) *ChatHandler {
	return &ChatHandler{hub: hub}
}

func (h *ChatHandler) HandleWebSocket(c *gin.Context) {
	userID := c.GetUint("user_id")

	// Get user info
	var user models.User
	if err := database.DB.First(&user, userID).Error; err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "用户不存在"})
		return
	}

	conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		return
	}

	client := &services.Client{
		Conn:     conn,
		UserID:   userID,
		Username: user.Username,
		Avatar:   user.Avatar,
		Rooms:    make(map[string]bool),
		Send:     make(chan []byte, 256),
	}

	go h.hub.HandleClient(client)
}

func (h *ChatHandler) GetRooms(c *gin.Context) {
	rooms, err := services.GetRooms()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "获取房间列表失败"})
		return
	}

	if rooms == nil {
		rooms = []models.ChatRoom{}
	}

	c.JSON(http.StatusOK, gin.H{"rooms": rooms})
}

func (h *ChatHandler) GetRoomMessages(c *gin.Context) {
	roomID, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的房间 ID"})
		return
	}

	before := c.Query("before")
	limitStr := c.DefaultQuery("limit", "50")
	limit, _ := strconv.Atoi(limitStr)

	messages, err := services.GetRoomMessages(uint(roomID), before, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "获取消息失败"})
		return
	}

	if messages == nil {
		messages = []models.ChatMessage{}
	}

	c.JSON(http.StatusOK, gin.H{"messages": messages})
}
