package router

import (
	"github.com/gin-gonic/gin"

	"redis-lab/server/config"
	"redis-lab/server/handlers"
	"redis-lab/server/middleware"
	"redis-lab/server/services"
)

func Setup(cfg *config.Config, redisProxy *services.RedisProxy, quotaService *services.QuotaService, chatHub *services.ChatHub) *gin.Engine {
	r := gin.Default()
	r.Use(middleware.CORSMiddleware())

	// Handlers
	authHandler := handlers.NewAuthHandler(cfg)
	connHandler := handlers.NewConnectionHandler(redisProxy)
	redisHandler := handlers.NewRedisExecHandler(redisProxy)
	quotaHandler := handlers.NewQuotaHandler(quotaService)
	chatHandler := handlers.NewChatHandler(chatHub)
	aiHandler := handlers.NewAIHandler(cfg)

	api := r.Group("/api")
	{
		// Public
		auth := api.Group("/auth")
		{
			auth.POST("/register", authHandler.Register)
			auth.POST("/login", authHandler.Login)
		}

		// Protected
		protected := api.Group("")
		protected.Use(middleware.AuthMiddleware(cfg))
		{
			protected.GET("/auth/me", authHandler.Me)

			// Quota
			protected.GET("/quota", quotaHandler.GetStatus)

			// Connections
			connections := protected.Group("/connections")
			{
				connections.GET("", connHandler.List)
				connections.POST("", connHandler.Create)
				connections.PUT("/:id", connHandler.Update)
				connections.DELETE("/:id", connHandler.Delete)
				connections.POST("/:id/test", connHandler.Test)
			}

			// Redis operations
			redis := protected.Group("/connections/:id")
			{
				redis.POST("/exec", redisHandler.ExecCommand)
				redis.GET("/keys", redisHandler.GetKeys)
				redis.GET("/keys/*key", redisHandler.GetKeyDetail)
				redis.DELETE("/keys/*key", redisHandler.DeleteKey)
				redis.GET("/info", redisHandler.GetServerInfo)
				redis.POST("/flush", redisHandler.FlushDB)
			}

			// Chat
			protected.GET("/rooms", chatHandler.GetRooms)
			protected.GET("/rooms/:id/messages", chatHandler.GetRoomMessages)
			protected.GET("/ws/chat", chatHandler.HandleWebSocket)

			// AI
			ai := protected.Group("/ai")
			{
				ai.POST("/chat", aiHandler.Chat)
				ai.GET("/conversations", aiHandler.GetConversations)
				ai.GET("/conversations/:id", aiHandler.GetConversation)
				ai.DELETE("/conversations/:id", aiHandler.DeleteConversation)
			}
		}
	}

	return r
}
