package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	"redis-lab/server/config"
	"redis-lab/server/database"
	"redis-lab/server/router"
	"redis-lab/server/services"
)

func main() {
	log.SetFlags(log.LstdFlags | log.Lshortfile)
	log.Println("🚀 RedisLab Server starting...")

	cfg := config.Load()
	log.Printf("Config: port=%s db=%s", cfg.Server.Port, cfg.Database.Path)
	log.Printf("Storage Redis: %s:%s", cfg.StorageRedis.Host, cfg.StorageRedis.Port)
	log.Printf("Learn Redis: %s:%s", cfg.LearnRedis.Host, cfg.LearnRedis.Port)

	// ---- Database ----
	if err := database.Init(cfg.Database.Path); err != nil {
		log.Fatalf("Database init failed: %v", err)
	}
	log.Println("SQLite ready")

	// ---- Storage Redis (server-side) ----
	storageRedis := services.NewStorageRedis(cfg.StorageRedis)
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	if err := storageRedis.Ping(ctx); err != nil {
		log.Printf("⚠️  Storage Redis not reachable: %v (continuing without cache)", err)
	} else {
		log.Println("Storage Redis connected")
	}
	cancel()
	defer storageRedis.Close()

	// ---- Core services ----
	quotaService := services.NewQuotaService()
	redisProxy := services.NewRedisProxy(quotaService)
	defer redisProxy.CloseAll()

	chatHub := services.NewChatHub()
	go chatHub.Run()
	log.Println("Chat hub started")

	// ---- HTTP server ----
	r := router.Setup(cfg, redisProxy, quotaService, chatHub)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		addr := fmt.Sprintf(":%s", cfg.Server.Port)
		log.Printf("✅ RedisLab listening on %s", addr)
		if err := r.Run(addr); err != nil {
			log.Fatalf("Server failed: %v", err)
		}
	}()

	<-quit
	log.Println("Shutting down...")
	redisProxy.CloseAll()
	log.Println("RedisLab Server stopped")
}
