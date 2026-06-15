package models

import "time"

type ChatRoom struct {
	ID          uint      `gorm:"primaryKey" json:"id"`
	Name        string    `gorm:"uniqueIndex;not null;size:64" json:"name"`
	Title       string    `gorm:"not null;size:128" json:"title"`
	Description string    `gorm:"size:512;default:''" json:"description"`
	CreatedAt   time.Time `json:"created_at"`
}
