package models

import "time"

type LedisConnection struct {
	ID        uint      `gorm:"primaryKey" json:"id"`
	UserID    uint      `gorm:"not null;index" json:"user_id"`
	Name      string    `gorm:"not null;size:128" json:"name"`
	Host      string    `gorm:"not null;size:256;default:127.0.0.1" json:"host"`
	Port      int       `gorm:"not null;default:6379" json:"port"`
	Password  string    `gorm:"size:256;default:''" json:"password,omitempty"`
	DBIndex   int       `gorm:"default:0" json:"db_index"`
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`
}

type CreateConnectionRequest struct {
	Name     string `json:"name" binding:"required"`
	Host     string `json:"host" binding:"required"`
	Port     int    `json:"port" binding:"required,min=1,max=65535"`
	Password string `json:"password"`
	DBIndex  int    `json:"db_index"`
}

type UpdateConnectionRequest struct {
	Name     string `json:"name"`
	Host     string `json:"host"`
	Port     int    `json:"port" binding:"omitempty,min=1,max=65535"`
	Password string `json:"password"`
	DBIndex  int    `json:"db_index"`
}

type TestConnectionResponse struct {
	Success bool   `json:"success"`
	Message string `json:"message"`
}
