package services

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"

	"redis-lab/server/config"
)

// StorageRedis provides server-side Redis operations (sessions, cache, etc.)
// This is separate from the "learning Redis" that users practice on.
type StorageRedis struct {
	client *redis.Client
}

func NewStorageRedis(cfg config.RedisConfig) *StorageRedis {
	addr := fmt.Sprintf("%s:%s", cfg.Host, cfg.Port)
	client := redis.NewClient(&redis.Options{
		Addr:         addr,
		DialTimeout:  3 * time.Second,
		ReadTimeout:  3 * time.Second,
		WriteTimeout: 3 * time.Second,
		PoolSize:     10,
		MinIdleConns: 2,
	})

	return &StorageRedis{client: client}
}

func (s *StorageRedis) Ping(ctx context.Context) error {
	return s.client.Ping(ctx).Err()
}

func (s *StorageRedis) Close() error {
	return s.client.Close()
}

// Cache user login token for fast validation (optional optimization over SQLite)
func (s *StorageRedis) CacheToken(ctx context.Context, token string, userID uint, ttl time.Duration) error {
	return s.client.Set(ctx, fmt.Sprintf("token:%s", token), userID, ttl).Err()
}

func (s *StorageRedis) GetTokenUser(ctx context.Context, token string) (uint, error) {
	val, err := s.client.Get(ctx, fmt.Sprintf("token:%s", token)).Uint64()
	if err != nil {
		return 0, err
	}
	return uint(val), nil
}

// Cache user session online status
func (s *StorageRedis) SetUserOnline(ctx context.Context, userID uint, ttl time.Duration) error {
	return s.client.Set(ctx, fmt.Sprintf("online:%d", userID), 1, ttl).Err()
}

func (s *StorageRedis) IsUserOnline(ctx context.Context, userID uint) bool {
	n, _ := s.client.Exists(ctx, fmt.Sprintf("online:%d", userID)).Result()
	return n > 0
}

// Cache frequently-accessed data to reduce SQLite load
func (s *StorageRedis) GetCached(ctx context.Context, key string) (string, error) {
	return s.client.Get(ctx, key).Result()
}

func (s *StorageRedis) SetCached(ctx context.Context, key, value string, ttl time.Duration) error {
	return s.client.Set(ctx, key, value, ttl).Err()
}

func (s *StorageRedis) DelCached(ctx context.Context, keys ...string) error {
	return s.client.Del(ctx, keys...).Err()
}
