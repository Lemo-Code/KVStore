package middleware

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"

	"redis-lab/server/config"
	"redis-lab/server/services"
)

func AuthMiddleware(cfg *config.Config) gin.HandlerFunc {
	authService := services.NewAuthService(cfg)

	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			// Check query parameter for WebSocket
			token := c.Query("token")
			if token == "" {
				c.JSON(http.StatusUnauthorized, gin.H{"error": "未提供认证令牌"})
				c.Abort()
				return
			}
			authHeader = "Bearer " + token
		}

		tokenStr := strings.TrimPrefix(authHeader, "Bearer ")
		if tokenStr == authHeader {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "无效的认证头"})
			c.Abort()
			return
		}

		userID, err := authService.ValidateToken(tokenStr)
		if err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "认证令牌无效或已过期"})
			c.Abort()
			return
		}

		c.Set("user_id", userID)
		c.Next()
	}
}

func CORSMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Header("Access-Control-Allow-Origin", "*")
		c.Header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Origin, Content-Type, Authorization")
		c.Header("Access-Control-Max-Age", "86400")

		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(http.StatusNoContent)
			return
		}

		c.Next()
	}
}
