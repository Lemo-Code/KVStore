package models

import "time"

type ChatMessage struct {
	ID        uint      `gorm:"primaryKey" json:"id"`
	RoomID    uint      `gorm:"not null;index" json:"room_id"`
	UserID    uint      `gorm:"not null;index" json:"user_id"`
	Content   string    `gorm:"not null;size:4096" json:"content"`
	CreatedAt time.Time `json:"created_at"`

	// Associations (loaded via Preload)
	User *User     `gorm:"foreignKey:UserID" json:"user,omitempty"`
	Room *ChatRoom `gorm:"foreignKey:RoomID" json:"room,omitempty"`
}
