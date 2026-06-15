package config

import (
	"os"
	"strconv"
)

type Config struct {
	Server      ServerConfig
	Database    DatabaseConfig
	StorageRedis RedisConfig
	LearnRedis  RedisConfig
	JWT         JWTConfig
	AI          AIConfig
}

type ServerConfig struct {
	Port string
}

type DatabaseConfig struct {
	Path string
}

type RedisConfig struct {
	Host string
	Port string
}

type JWTConfig struct {
	Secret     string
	ExpireHour int
}

type AIConfig struct {
	APIKey  string
	BaseURL string
	Model   string
}

func Load() *Config {
	return &Config{
		Server: ServerConfig{
			Port: getEnv("SERVER_PORT", "8080"),
		},
		Database: DatabaseConfig{
			Path: getEnv("DB_PATH", "./data/redislab.db"),
		},
		StorageRedis: RedisConfig{
			Host: getEnv("STORAGE_REDIS_HOST", "127.0.0.1"),
			Port: getEnv("STORAGE_REDIS_PORT", "6379"),
		},
		LearnRedis: RedisConfig{
			Host: getEnv("LEARN_REDIS_HOST", "127.0.0.1"),
			Port: getEnv("LEARN_REDIS_PORT", "6380"),
		},
		JWT: JWTConfig{
			Secret:     getEnv("JWT_SECRET", "redis-lab-secret-key-change-in-production"),
			ExpireHour: getEnvInt("JWT_EXPIRE_HOUR", 24),
		},
		AI: AIConfig{
			APIKey:  getEnv("AI_API_KEY", ""),
			BaseURL: getEnv("AI_BASE_URL", "https://api.anthropic.com"),
			Model:   getEnv("AI_MODEL", "claude-sonnet-4-6"),
		},
	}
}

func getEnv(key, defaultVal string) string {
	if val := os.Getenv(key); val != "" {
		return val
	}
	return defaultVal
}

func getEnvInt(key string, defaultVal int) int {
	if val := os.Getenv(key); val != "" {
		if i, err := strconv.Atoi(val); err == nil {
			return i
		}
	}
	return defaultVal
}
