package handlers

import (
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"

	"redis-lab/server/database"
	"redis-lab/server/models"
	"redis-lab/server/services"
)

type RedisExecHandler struct {
	redisProxy *services.RedisProxy
}

func NewRedisExecHandler(redisProxy *services.RedisProxy) *RedisExecHandler {
	return &RedisExecHandler{redisProxy: redisProxy}
}

func (h *RedisExecHandler) ExecCommand(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid connection id"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "connection not found"})
		return
	}

	var req struct {
		Command string `json:"command" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	ctx := services.ExecContext{
		UserID:   userID,
		ConnID:   uint(id),
		Host:     conn.Host,
		Port:     conn.Port,
		Password: conn.Password,
		DBIndex:  conn.DBIndex,
	}

	result, err := h.redisProxy.ExecCommand(ctx, req.Command)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"result": result})
}

func (h *RedisExecHandler) GetKeys(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid connection id"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "connection not found"})
		return
	}

	ctx := services.ExecContext{
		UserID:   userID,
		ConnID:   uint(id),
		Host:     conn.Host,
		Port:     conn.Port,
		Password: conn.Password,
		DBIndex:  conn.DBIndex,
	}

	keys, err := h.redisProxy.GetKeys(ctx, c.Query("pattern"))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"keys": keys})
}

func (h *RedisExecHandler) GetKeyDetail(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid connection id"})
		return
	}

	key := c.Param("key")
	if key == "" || key == "/" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "key is required"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "connection not found"})
		return
	}

	ctx := services.ExecContext{
		UserID:   userID,
		ConnID:   uint(id),
		Host:     conn.Host,
		Port:     conn.Port,
		Password: conn.Password,
		DBIndex:  conn.DBIndex,
	}

	detail, err := h.redisProxy.GetKeyDetail(ctx, key)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"detail": detail})
}

func (h *RedisExecHandler) GetServerInfo(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid connection id"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "connection not found"})
		return
	}

	ctx := services.ExecContext{
		UserID:   userID,
		ConnID:   uint(id),
		Host:     conn.Host,
		Port:     conn.Port,
		Password: conn.Password,
		DBIndex:  conn.DBIndex,
	}

	info, err := h.redisProxy.GetServerInfo(ctx)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"info": info})
}

func (h *RedisExecHandler) DeleteKey(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid connection id"})
		return
	}

	key := c.Param("key")
	if key == "" || key == "/" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "key is required"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "connection not found"})
		return
	}

	ctx := services.ExecContext{
		UserID:   userID,
		ConnID:   uint(id),
		Host:     conn.Host,
		Port:     conn.Port,
		Password: conn.Password,
		DBIndex:  conn.DBIndex,
	}

	deleted, err := h.redisProxy.DeleteKey(ctx, key)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"deleted": deleted})
}

// FlushDB flushes only the user's namespace.
func (h *RedisExecHandler) FlushDB(c *gin.Context) {
	userID := c.GetUint("user_id")
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid connection id"})
		return
	}

	var conn models.LedisConnection
	if err := database.DB.Where("id = ? AND user_id = ?", id, userID).First(&conn).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "connection not found"})
		return
	}

	ctx := services.ExecContext{
		UserID:   userID,
		ConnID:   uint(id),
		Host:     conn.Host,
		Port:     conn.Port,
		Password: conn.Password,
		DBIndex:  conn.DBIndex,
	}

	deleted, err := h.redisProxy.FlushUserNamespace(ctx)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"deleted": deleted, "message": "Your namespace has been flushed"})
}
