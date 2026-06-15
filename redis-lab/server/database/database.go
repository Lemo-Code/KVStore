package database

import (
	"os"
	"path/filepath"

	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"

	"redis-lab/server/models"
)

var DB *gorm.DB

func Init(dbPath string) error {
	dir := filepath.Dir(dbPath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}

	var err error
	DB, err = gorm.Open(sqlite.Open(dbPath), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Info),
	})
	if err != nil {
		return err
	}

	// AutoMigrate
	if err := DB.AutoMigrate(
		&models.User{},
		&models.UserQuota{},
		&models.LedisConnection{},
		&models.ChatRoom{},
		&models.ChatMessage{},
		&models.AIConversation{},
		&models.AIMessage{},
	); err != nil {
		return err
	}

	// Seed default chat rooms
	seedChatRooms()

	return nil
}

func seedChatRooms() {
	rooms := []models.ChatRoom{
		{Name: "general", Title: "综合讨论", Description: "Redis 学习交流综合频道"},
		{Name: "strings", Title: "字符串命令", Description: "GET/SET/INCR/APPEND 等字符串命令讨论"},
		{Name: "keys", Title: "Key 管理", Description: "DEL/EXISTS/EXPIRE/TTL/KEYS 等 Key 操作讨论"},
		{Name: "advanced", Title: "进阶话题", Description: "Redis 底层数据结构、持久化、集群等进阶话题"},
	}

	for _, room := range rooms {
		DB.Where("name = ?", room.Name).FirstOrCreate(&room)
	}
}
