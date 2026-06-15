package handlers

import (
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"

	"redis-lab/server/database"
	"redis-lab/server/models"
	"redis-lab/server/services"
)

type ConnectionHandler struct {
	redisProxy *services.RedisProxy
}

func NewConnectionHandler(redisProxy *services.RedisProxy) *ConnectionHandler {
	return &ConnectionHandler{redisProxy: redisProxy}
}

func (h *ConnectionHandler) List(c *gin.Context) {
	userID := c.GetUint("user_id")
	var connections []models.LedisConnection
	if err := database.DB.Where("user_id = ?", userID).Find(&connections).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "获取连接列表失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"connections": connections})
}

func (h *ConnectionHandler) Create(c *gin.Context) {
	userID := c.GetUint("user_id")
	var req models.CreateConnectionRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	conn := models.LedisConnection{
		UserID:   userID,
		Name:     req.Name,
		Host:     req.Host,
		Port:     req.Port,
		Password: req.Password,
		DBIndex:  req.DBIndex,
	}

	if err := database.DB.Create(&conn).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "创建连接失败"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{"connection": conn})
}

func (h *ConnectionHandler) Update(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的连接 ID"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "连接不存在"})
		return
	}

	var req models.UpdateConnectionRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	updates := map[string]interface{}{}
	if req.Name != "" {
		updates["name"] = req.Name
	}
	if req.Host != "" {
		updates["host"] = req.Host
	}
	if req.Port > 0 {
		updates["port"] = req.Port
	}
	if req.Password != "" {
		updates["password"] = req.Password
	}
	updates["db_index"] = req.DBIndex

	if err := database.DB.Model(&conn).Updates(updates).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "更新连接失败"})
		return
	}

	// Refresh and remove old client
	h.redisProxy.RemoveClient(uint(id))
	database.DB.First(&conn, id)

	c.JSON(http.StatusOK, gin.H{"connection": conn})
}

func (h *ConnectionHandler) Delete(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的连接 ID"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "连接不存在"})
		return
	}

	// Close and remove from proxy pool
	h.redisProxy.RemoveClient(uint(id))

	if err := database.DB.Delete(&conn).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "删除连接失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "连接已删除"})
}

func (h *ConnectionHandler) Test(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的连接 ID"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "连接不存在"})
		return
	}

	err = h.redisProxy.TestConnection(conn.Host, conn.Port, conn.Password)
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"success": false, "message": "连接失败: " + err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"success": true, "message": "连接成功"})
}
