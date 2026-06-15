package services

import (
	"context"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"

	"redis-lab/server/models"
)

// CommandResult represents the result of executing a Redis command.
type CommandResult struct {
	Type  string      `json:"type"`
	Value interface{} `json:"value"`
}

// ExecContext holds all context needed to execute a Redis command for a specific user.
type ExecContext struct {
	UserID   uint
	ConnID   uint
	Host     string
	Port     int
	Password string
	DBIndex  int
}

// RedisProxy manages connections to Ledis instances with per-user namespace isolation.
type RedisProxy struct {
	clients      map[uint]*redis.Client
	mu           sync.RWMutex
	quotaService *QuotaService
}

func NewRedisProxy(qs *QuotaService) *RedisProxy {
	return &RedisProxy{
		clients:      make(map[uint]*redis.Client),
		quotaService: qs,
	}
}

func (p *RedisProxy) getOrCreateClient(id uint, host string, port int, password string, dbIndex int) *redis.Client {
	p.mu.RLock()
	client, exists := p.clients[id]
	p.mu.RUnlock()
	if exists {
		return client
	}

	p.mu.Lock()
	defer p.mu.Unlock()
	if client, exists := p.clients[id]; exists {
		return client
	}

	addr := fmt.Sprintf("%s:%d", host, port)
	client = redis.NewClient(&redis.Options{
		Addr:         addr,
		Password:     password,
		DB:           dbIndex,
		DialTimeout:  5 * time.Second,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
		PoolSize:     5,
		MinIdleConns: 1,
	})
	p.clients[id] = client
	return client
}

func (p *RedisProxy) RemoveClient(id uint) {
	p.mu.Lock()
	defer p.mu.Unlock()
	if client, exists := p.clients[id]; exists {
		client.Close()
		delete(p.clients, id)
	}
}

func (p *RedisProxy) TestConnection(host string, port int, password string) error {
	addr := fmt.Sprintf("%s:%d", host, port)
	client := redis.NewClient(&redis.Options{
		Addr:        addr,
		Password:    password,
		DialTimeout: 3 * time.Second,
		ReadTimeout: 3 * time.Second,
	})
	defer client.Close()
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	return client.Ping(ctx).Err()
}

// ExecCommand executes a Redis command with namespace isolation and quota enforcement.
func (p *RedisProxy) ExecCommand(ctx ExecContext, command string) (*CommandResult, error) {
	client := p.getOrCreateClient(ctx.ConnID, ctx.Host, ctx.Port, ctx.Password, ctx.DBIndex)

	// Parse command
	args := parseCommand(command)
	if len(args) == 0 {
		return nil, fmt.Errorf("empty command")
	}

	cmdUpper := strings.ToUpper(args[0])

	// Block dangerous commands that affect the whole instance
	if isBlockedGlobal(cmdUpper) {
		return &CommandResult{Type: "error",
			Value: fmt.Sprintf("ERR command '%s' is blocked in sandbox mode", cmdUpper),
		}, nil
	}

	// Apply namespace to keys for write commands
	namespacedArgs := p.applyNamespace(ctx.UserID, cmdUpper, args)

	// Check quota before execution
	if err := p.quotaService.CheckCommand(ctx.UserID, namespacedArgs); err != nil {
		return &CommandResult{Type: "error", Value: err.Error()}, nil
	}

	// Execute
	execCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	rawArgs := make([]interface{}, len(namespacedArgs))
	for i, a := range namespacedArgs {
		rawArgs[i] = a
	}

	result, err := client.Do(execCtx, rawArgs...).Result()
	if err != nil {
		// Track even failed commands? No — only track successes.
		return &CommandResult{Type: "error", Value: err.Error()}, nil
	}

	// Track quota usage on success
	p.quotaService.TrackCommand(ctx.UserID, namespacedArgs, result)

	// Strip namespace from results for read commands
	formatted := formatResult(result)
	formatted = p.stripNamespaceFromResult(ctx.UserID, formatted)
	return formatted, nil
}

// GetKeys retrieves keys in user's namespace.
func (p *RedisProxy) GetKeys(ctx ExecContext, pattern string) ([]string, error) {
	client := p.getOrCreateClient(ctx.ConnID, ctx.Host, ctx.Port, ctx.Password, ctx.DBIndex)

	if pattern == "" {
		pattern = "*"
	}

	// Prepend namespace to pattern
	nsPattern := models.KeyNamespacePrefix(ctx.UserID) + pattern

	execCtx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	var keys []string
	var cursor uint64

	for {
		batch, cur, err := client.Scan(execCtx, cursor, nsPattern, 200).Result()
		if err != nil {
			return nil, err
		}
		for _, k := range batch {
			// Strip namespace prefix before returning to user
			keys = append(keys, models.StripNamespace(ctx.UserID, k))
		}
		cursor = cur
		if cursor == 0 {
			break
		}
	}

	if keys == nil {
		keys = []string{}
	}
	return keys, nil
}

// GetKeyDetail gets type, TTL, and value of a key in user's namespace.
func (p *RedisProxy) GetKeyDetail(ctx ExecContext, key string) (map[string]interface{}, error) {
	client := p.getOrCreateClient(ctx.ConnID, ctx.Host, ctx.Port, ctx.Password, ctx.DBIndex)
	nsKey := models.ApplyNamespace(ctx.UserID, key)

	execCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	pipe := client.Pipeline()
	typeCmd := pipe.Type(execCtx, nsKey)
	ttlCmd := pipe.TTL(execCtx, nsKey)
	getCmd := pipe.Get(execCtx, nsKey)

	_, err := pipe.Exec(execCtx)
	if err != nil && err != redis.Nil {
		if strings.Contains(err.Error(), "no such key") {
			return nil, fmt.Errorf("key not found")
		}
	}

	detail := map[string]interface{}{
		"key":  key, // return original key (without namespace)
		"type": typeCmd.Val(),
	}

	ttl := ttlCmd.Val().Seconds()
	if ttl >= 0 {
		detail["ttl"] = int64(ttl)
	} else {
		detail["ttl"] = int64(-1)
	}

	val, err := getCmd.Result()
	if err == nil {
		detail["value"] = val
	} else if err == redis.Nil {
		detail["value"] = nil
	}

	return detail, nil
}

// GetServerInfo returns INFO from server.
func (p *RedisProxy) GetServerInfo(ctx ExecContext) (string, error) {
	client := p.getOrCreateClient(ctx.ConnID, ctx.Host, ctx.Port, ctx.Password, ctx.DBIndex)
	execCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	return client.Info(execCtx).Result()
}

// DeleteKey deletes a key in user's namespace.
func (p *RedisProxy) DeleteKey(ctx ExecContext, key string) (int64, error) {
	client := p.getOrCreateClient(ctx.ConnID, ctx.Host, ctx.Port, ctx.Password, ctx.DBIndex)
	nsKey := models.ApplyNamespace(ctx.UserID, key)

	execCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	return client.Del(execCtx, nsKey).Result()
}

// FlushUserNamespace clears all keys in the user's namespace.
func (p *RedisProxy) FlushUserNamespace(ctx ExecContext) (int64, error) {
	client := p.getOrCreateClient(ctx.ConnID, ctx.Host, ctx.Port, ctx.Password, ctx.DBIndex)
	nsPattern := models.KeyNamespacePrefix(ctx.UserID) + "*"

	execCtx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	var totalDeleted int64
	var cursor uint64

	for {
		keys, cur, err := client.Scan(execCtx, cursor, nsPattern, 500).Result()
		if err != nil {
			return totalDeleted, err
		}
		if len(keys) > 0 {
			delCount, err := client.Del(execCtx, keys...).Result()
			if err != nil {
				return totalDeleted, err
			}
			totalDeleted += delCount
		}
		cursor = cur
		if cursor == 0 {
			break
		}
	}

	return totalDeleted, nil
}

func (p *RedisProxy) CloseAll() {
	p.mu.Lock()
	defer p.mu.Unlock()
	for id, client := range p.clients {
		client.Close()
		delete(p.clients, id)
	}
}

// ---- Namespace helpers ----

// applyNamespace prefixes keys with the user's namespace for commands that operate on keys.
func (p *RedisProxy) applyNamespace(userID uint, cmd string, args []string) []string {
	result := make([]string, len(args))
	result[0] = args[0] // command name stays the same

	switch cmd {
	// Commands where first arg is a key
	case "GET", "SET", "SETNX", "SETEX", "PSETEX", "DEL", "UNLINK",
		"EXISTS", "TYPE", "EXPIRE", "EXPIREAT", "PEXPIRE", "PEXPIREAT",
		"TTL", "PTTL", "PERSIST", "INCR", "INCRBY", "DECR", "DECRBY",
		"APPEND", "STRLEN", "GETSET", "GETRANGE", "SETRANGE",
		"GETDEL", "GETEX", "RENAME", "RENAMENX":
		if len(args) >= 2 {
			result[1] = models.ApplyNamespace(userID, args[1])
		}
		// RENAME has second key arg
		if (cmd == "RENAME" || cmd == "RENAMENX") && len(args) >= 3 {
			result[2] = models.ApplyNamespace(userID, args[2])
		}
		copyRemaining(result, args, 2, cmd)

	// MGET/MSET: multiple keys
	case "MGET":
		for i := 1; i < len(args); i++ {
			result[i] = models.ApplyNamespace(userID, args[i])
		}
	case "MSET", "MSETNX":
		for i := 1; i < len(args); i += 2 {
			result[i] = models.ApplyNamespace(userID, args[i])
			if i+1 < len(args) {
				result[i+1] = args[i+1] // value unchanged
			}
		}

	// Commands with no key args — pass through
	case "PING", "ECHO", "COMMAND", "INFO", "DBSIZE", "RANDOMKEY",
		"FLUSHDB", "FLUSHALL", "SELECT", "KEYS":
		// FLUSHDB is intercepted at higher level to flush only user namespace
		copyRemaining(result, args, 1, cmd)

	// Default: assume first arg after command is a key
	default:
		if len(args) >= 2 {
			result[1] = models.ApplyNamespace(userID, args[1])
		}
		copyRemaining(result, args, 2, cmd)
	}

	return result
}

func copyRemaining(dst, src []string, start int, cmd string) {
	for i := start; i < len(src) && i < len(dst); i++ {
		dst[i] = src[i]
	}
}

// stripNamespaceFromResult removes namespace prefix from keys in results.
func (p *RedisProxy) stripNamespaceFromResult(userID uint, r *CommandResult) *CommandResult {
	prefix := models.KeyNamespacePrefix(userID)

	if r.Type == "string" {
		if s, ok := r.Value.(string); ok {
			r.Value = models.StripNamespace(userID, s)
		}
	}
	if r.Type == "array" {
		if arr, ok := r.Value.([]interface{}); ok {
			for i, item := range arr {
				if s, ok := item.(string); ok && strings.HasPrefix(s, prefix) {
					arr[i] = models.StripNamespace(userID, s)
				}
			}
		}
	}
	return r
}

// isBlockedGlobal blocks commands that would affect the entire Ledis instance.
func isBlockedGlobal(cmd string) bool {
	blocked := map[string]bool{
		"FLUSHALL": true,
		"SHUTDOWN": true,
		"CONFIG":   true,
		"DEBUG":    true,
		"SAVE":     true,
		"BGSAVE":   true,
		"BGREWRITEAOF": true,
		"CLIENT":   true,
		"CLUSTER":  true,
		"MONITOR":  true,
		"SYNC":     true,
		"REPLICAOF": true,
		"SLAVEOF":  true,
		"MIGRATE":  true,
		"RESTORE":  true,
		"SCRIPT":   true,
		"EVAL":     true,
		"EVALSHA":  true,
	}
	return blocked[cmd]
}

// parseCommand parses a Redis command string into args, respecting quotes.
func parseCommand(command string) []string {
	var args []string
	var current strings.Builder
	inDouble := false
	inSingle := false

	for i := 0; i < len(command); i++ {
		ch := command[i]
		if inDouble {
			if ch == '"' {
				inDouble = false
			} else {
				current.WriteByte(ch)
			}
		} else if inSingle {
			if ch == '\'' {
				inSingle = false
			} else {
				current.WriteByte(ch)
			}
		} else {
			if ch == '"' {
				inDouble = true
			} else if ch == '\'' {
				inSingle = true
			} else if ch == ' ' || ch == '\t' {
				if current.Len() > 0 {
					args = append(args, current.String())
					current.Reset()
				}
			} else {
				current.WriteByte(ch)
			}
		}
	}
	if current.Len() > 0 {
		args = append(args, current.String())
	}
	return args
}

func formatResult(result interface{}) *CommandResult {
	if result == nil {
		return &CommandResult{Type: "null", Value: nil}
	}
	switch v := result.(type) {
	case string:
		return &CommandResult{Type: "string", Value: v}
	case int64:
		return &CommandResult{Type: "integer", Value: v}
	case []interface{}:
		items := make([]interface{}, len(v))
		for i, item := range v {
			if s, ok := item.(string); ok {
				items[i] = s
			} else {
				items[i] = fmt.Sprintf("%v", item)
			}
		}
		return &CommandResult{Type: "array", Value: items}
	case []string:
		items := make([]interface{}, len(v))
		for i, s := range v {
			items[i] = s
		}
		return &CommandResult{Type: "array", Value: items}
	default:
		return &CommandResult{Type: "string", Value: fmt.Sprintf("%v", v)}
	}
}
