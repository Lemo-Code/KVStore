package models

import (
	"fmt"
	"time"
)

// UserQuota defines per-user resource limits on the shared Ledis instance.
type UserQuota struct {
	ID           uint      `gorm:"primaryKey" json:"id"`
	UserID       uint      `gorm:"uniqueIndex;not null" json:"user_id"`
	MaxKeys      int64     `gorm:"not null;default:100" json:"max_keys"`
	MaxValueSize int64     `gorm:"not null;default:1048576" json:"max_value_size"` // bytes (default 1MB)
	MaxMemory    int64     `gorm:"not null;default:104857600" json:"max_memory"`   // bytes (default 100MB)
	RateLimit    int       `gorm:"not null;default:60" json:"rate_limit"`           // commands per minute
	UsedKeys     int64     `gorm:"not null;default:0" json:"used_keys"`
	UsedMemory   int64     `gorm:"not null;default:0" json:"used_memory"`
	TotalCmds    int64     `gorm:"not null;default:0" json:"total_cmds"`
	CreatedAt    time.Time `json:"created_at"`
	UpdatedAt    time.Time `json:"updated_at"`
}

// QuotaStatus represents the current resource usage for display.
type QuotaStatus struct {
	MaxKeys       int64   `json:"max_keys"`
	UsedKeys      int64   `json:"used_keys"`
	KeysPercent   float64 `json:"keys_percent"`
	MaxValueSize  int64   `json:"max_value_size"`
	MaxMemory     int64   `json:"max_memory"`
	UsedMemory    int64   `json:"used_memory"`
	MemoryPercent float64 `json:"memory_percent"`
	RateLimit     int     `json:"rate_limit"`
	TotalCmds     int64   `json:"total_cmds"`
}

// KeyNamespacePrefix returns the Ledis key prefix for a user.
func KeyNamespacePrefix(userID uint) string {
	return fmt.Sprintf("u:%d:", userID)
}

// ApplyNamespace wraps a key with the user's namespace prefix.
func ApplyNamespace(userID uint, key string) string {
	return KeyNamespacePrefix(userID) + key
}

// StripNamespace removes the user namespace prefix from a key.
func StripNamespace(userID uint, key string) string {
	prefix := KeyNamespacePrefix(userID)
	if len(key) > len(prefix) && key[:len(prefix)] == prefix {
		return key[len(prefix):]
	}
	return key
}
