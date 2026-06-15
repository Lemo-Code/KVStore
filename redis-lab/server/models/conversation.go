package models

import "time"

type AIConversation struct {
	ID        uint      `gorm:"primaryKey" json:"id"`
	UserID    uint      `gorm:"not null;index" json:"user_id"`
	Title     string    `gorm:"size:256;default:'新对话'" json:"title"`
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`

	Messages []AIMessage `gorm:"foreignKey:ConversationID" json:"messages,omitempty"`
}

type AIMessage struct {
	ID             uint      `gorm:"primaryKey" json:"id"`
	ConversationID uint      `gorm:"not null;index" json:"conversation_id"`
	Role           string    `gorm:"not null;size:16;check:role IN ('user','assistant')" json:"role"`
	Content        string    `gorm:"not null;size:16384" json:"content"`
	CreatedAt      time.Time `json:"created_at"`
}

type AIChatRequest struct {
	Message        string `json:"message" binding:"required"`
	ConversationID *uint  `json:"conversation_id"`
	ConnectionID   *uint  `json:"connection_id"` // optional: provide Ledis context
}
