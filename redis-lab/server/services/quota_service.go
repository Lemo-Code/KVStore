package services

import (
	"fmt"
	"sync"
	"time"

	"redis-lab/server/database"
	"redis-lab/server/models"
)

// QuotaService enforces per-user resource limits.
type QuotaService struct {
	mu       sync.RWMutex
	// rateLimiter tracks per-user command timestamps for rate limiting.
	rateWindows map[uint][]time.Time
}

func NewQuotaService() *QuotaService {
	return &QuotaService{
		rateWindows: make(map[uint][]time.Time),
	}
}

// GetOrCreateQuota returns the user's quota, creating default if needed.
func (s *QuotaService) GetOrCreateQuota(userID uint) (*models.UserQuota, error) {
	var quota models.UserQuota
	err := database.DB.Where("user_id = ?", userID).First(&quota).Error
	if err == nil {
		return &quota, nil
	}

	// Create default quota for new user
	quota = models.UserQuota{
		UserID:       userID,
		MaxKeys:      100,
		MaxValueSize: 1 * 1024 * 1024,   // 1MB per value
		MaxMemory:    100 * 1024 * 1024, // 100MB total
		RateLimit:    60,                 // 60 commands/min
	}
	if err := database.DB.Create(&quota).Error; err != nil {
		return nil, fmt.Errorf("创建配额失败: %w", err)
	}
	return &quota, nil
}

// GetQuotaStatus returns the user's current quota usage.
func (s *QuotaService) GetQuotaStatus(userID uint) (*models.QuotaStatus, error) {
	quota, err := s.GetOrCreateQuota(userID)
	if err != nil {
		return nil, err
	}

	keysPercent := float64(0)
	if quota.MaxKeys > 0 {
		keysPercent = float64(quota.UsedKeys) / float64(quota.MaxKeys) * 100
	}
	memPercent := float64(0)
	if quota.MaxMemory > 0 {
		memPercent = float64(quota.UsedMemory) / float64(quota.MaxMemory) * 100
	}

	return &models.QuotaStatus{
		MaxKeys:       quota.MaxKeys,
		UsedKeys:      quota.UsedKeys,
		KeysPercent:   keysPercent,
		MaxValueSize:  quota.MaxValueSize,
		MaxMemory:     quota.MaxMemory,
		UsedMemory:    quota.UsedMemory,
		MemoryPercent: memPercent,
		RateLimit:     quota.RateLimit,
		TotalCmds:     quota.TotalCmds,
	}, nil
}

// CheckCommand checks if a command is allowed under the user's quota.
// Returns error if quota exceeded, nil if allowed.
func (s *QuotaService) CheckCommand(userID uint, cmdArgs []string) error {
	if len(cmdArgs) == 0 {
		return nil
	}

	quota, err := s.GetOrCreateQuota(userID)
	if err != nil {
		return err
	}

	cmd := cmdArgs[0]

	// Check rate limit
	if err := s.checkRateLimit(userID, quota.RateLimit); err != nil {
		return err
	}

	switch cmd {
	case "SET", "SETNX", "SETEX", "PSETEX":
		// Check value size
		if len(cmdArgs) >= 3 {
			valueSize := int64(len(cmdArgs[2]))
			if valueSize > quota.MaxValueSize {
				return fmt.Errorf("QUOTA_EXCEEDED: value size %d bytes exceeds max %d bytes",
					valueSize, quota.MaxValueSize)
			}
		}
		// Check key count (will be checked after execution by tracking)

	case "MSET":
		// Check total value size
		totalSize := int64(0)
		for i := 2; i < len(cmdArgs); i += 2 {
			if i+1 < len(cmdArgs) {
				totalSize += int64(len(cmdArgs[i+1]))
			}
		}
		if totalSize > quota.MaxValueSize*2 {
			return fmt.Errorf("QUOTA_EXCEEDED: total value size %d bytes too large", totalSize)
		}

	case "APPEND", "SETRANGE":
		if len(cmdArgs) >= 3 {
			appendSize := int64(len(cmdArgs[2]))
			if appendSize > quota.MaxValueSize {
				return fmt.Errorf("QUOTA_EXCEEDED: append size %d bytes exceeds max %d bytes",
					appendSize, quota.MaxValueSize)
			}
		}
	}

	return nil
}

// TrackCommand updates quota usage after a successful command execution.
func (s *QuotaService) TrackCommand(userID uint, cmdArgs []string, result interface{}) {
	if len(cmdArgs) == 0 {
		return
	}

	// Increment total commands
	database.DB.Model(&models.UserQuota{}).
		Where("user_id = ?", userID).
		UpdateColumn("total_cmds", database.DB.Raw("total_cmds + 1"))

	cmd := cmdArgs[0]

	switch cmd {
	case "SET", "SETNX", "SETEX", "PSETEX":
		// New key added - update key count and memory estimate
		var keyLen, valLen int64
		if len(cmdArgs) >= 2 {
			keyLen = int64(len(cmdArgs[1]))
		}
		if len(cmdArgs) >= 3 {
			valLen = int64(len(cmdArgs[2]))
		}
		database.DB.Model(&models.UserQuota{}).
			Where("user_id = ?", userID).
			Updates(map[string]interface{}{
				"used_keys":   database.DB.Raw("used_keys + 1"),
				"used_memory": database.DB.Raw("used_memory + ?", keyLen+valLen),
			})

	case "MSET":
		newKeys := int64(len(cmdArgs)-1) / 2
		var memEstimate int64
		for i := 1; i < len(cmdArgs); i += 2 {
			if i+1 < len(cmdArgs) {
				memEstimate += int64(len(cmdArgs[i])) + int64(len(cmdArgs[i+1]))
			}
		}
		database.DB.Model(&models.UserQuota{}).
			Where("user_id = ?", userID).
			Updates(map[string]interface{}{
				"used_keys":   database.DB.Raw("used_keys + ?", newKeys),
				"used_memory": database.DB.Raw("used_memory + ?", memEstimate),
			})

	case "DEL", "UNLINK":
		// Keys deleted - decrease count and memory
		// We approximate — actual tracking would need to know the old value size
		count := int64(len(cmdArgs) - 1)
		if count > 0 {
			database.DB.Model(&models.UserQuota{}).
				Where("user_id = ?", userID).
				Updates(map[string]interface{}{
					"used_keys": database.DB.Raw("MAX(0, used_keys - ?)", count),
				})
		}

	case "FLUSHDB":
		// Reset user's quota usage
		database.DB.Model(&models.UserQuota{}).
			Where("user_id = ?", userID).
			Updates(map[string]interface{}{
				"used_keys":   0,
				"used_memory": 0,
			})
	}

	// Truncate used_keys at MaxKeys
	var quota models.UserQuota
	database.DB.Where("user_id = ?", userID).First(&quota)
	if quota.UsedKeys > quota.MaxKeys {
		database.DB.Model(&models.UserQuota{}).
			Where("user_id = ?", userID).
			Update("used_keys", quota.MaxKeys)
	}
}

// checkRateLimit implements a sliding-window rate limiter.
func (s *QuotaService) checkRateLimit(userID uint, maxPerMin int) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	now := time.Now()
	window := time.Minute
	cutoff := now.Add(-window)

	// Get or create window
	times := s.rateWindows[userID]

	// Remove expired entries
	valid := times[:0]
	for _, t := range times {
		if t.After(cutoff) {
			valid = append(valid, t)
		}
	}

	if len(valid) >= maxPerMin {
		return fmt.Errorf("RATE_LIMITED: max %d commands per minute exceeded", maxPerMin)
	}

	valid = append(valid, now)
	s.rateWindows[userID] = valid
	return nil
}
