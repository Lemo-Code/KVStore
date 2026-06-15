package models

import "time"

type User struct {
	ID        uint      `gorm:"primaryKey" json:"id"`
	Username  string    `gorm:"uniqueIndex;not null;size:64" json:"username"`
	Email     string    `gorm:"uniqueIndex;not null;size:128" json:"email"`
	Password  string    `gorm:"not null;size:256" json:"-"`
	Avatar    string    `gorm:"size:512;default:''" json:"avatar"`
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`
}

type RegisterRequest struct {
	Username string `json:"username" binding:"required,min=2,max=64"`
	Email    string `json:"email" binding:"required,email,max=128"`
	Password string `json:"password" binding:"required,min=6,max=128"`
}

type LoginRequest struct {
	Username string `json:"username" binding:"required"`
	Password string `json:"password" binding:"required"`
}

type LoginResponse struct {
	Token string `json:"token"`
	User  User   `json:"user"`
}
