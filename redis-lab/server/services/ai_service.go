package services

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"redis-lab/server/config"
	"redis-lab/server/database"
	"redis-lab/server/models"
)

type AIService struct {
	cfg        *config.Config
	httpClient *http.Client
}

func NewAIService(cfg *config.Config) *AIService {
	return &AIService{
		cfg: cfg,
		httpClient: &http.Client{
			Timeout: 120 * time.Second,
		},
	}
}

type anthropicMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type anthropicContent struct {
	Type string `json:"type"`
	Text string `json:"text"`
}

type anthropicRequest struct {
	Model     string             `json:"model"`
	MaxTokens int                `json:"max_tokens"`
	System    string             `json:"system"`
	Messages  []anthropicMessage `json:"messages"`
	Stream    bool               `json:"stream,omitempty"`
}

type anthropicContentBlock struct {
	Type string `json:"type"`
	Text string `json:"text"`
}

type anthropicStreamEvent struct {
	Type         string                 `json:"type"`
	ContentBlock *anthropicContentBlock `json:"content_block,omitempty"`
	Delta        *struct {
		Type string `json:"type"`
		Text string `json:"text"`
	} `json:"delta,omitempty"`
}

// Chat sends a message to Claude and returns a channel to stream tokens.
func (s *AIService) Chat(userID uint, req models.AIChatRequest, aiCtx AIChatContext) (<-chan string, <-chan error, error) {
	// Get or create conversation
	var conversationID uint
	if req.ConversationID != nil {
		conversationID = *req.ConversationID
	} else {
		conv := models.AIConversation{
			UserID: userID,
			Title:  "新对话",
		}
		if err := database.DB.Create(&conv).Error; err != nil {
			return nil, nil, fmt.Errorf("创建会话失败: %w", err)
		}
		conversationID = conv.ID
	}

	// Save user message
	userMsg := models.AIMessage{
		ConversationID: conversationID,
		Role:          "user",
		Content:       req.Message,
	}
	database.DB.Create(&userMsg)

	// Update conversation title from first message
	var msgCount int64
	database.DB.Model(&models.AIMessage{}).Where("conversation_id = ?", conversationID).Count(&msgCount)
	if msgCount <= 1 {
		title := req.Message
		if len([]rune(title)) > 50 {
			title = string([]rune(title)[:50]) + "..."
		}
		database.DB.Model(&models.AIConversation{}).Where("id = ?", conversationID).Update("title", title)
	}

	database.DB.Model(&models.AIConversation{}).Where("id = ?", conversationID).Update("updated_at", time.Now())

	// Load conversation history
	var historyMessages []models.AIMessage
	database.DB.Where("conversation_id = ?", conversationID).
		Order("created_at ASC").
		Find(&historyMessages)

	// Build messages array for API
	var apiMessages []anthropicMessage
	for _, m := range historyMessages {
		apiMessages = append(apiMessages, anthropicMessage{
			Role:    m.Role,
			Content: m.Content,
		})
	}

	// Build system prompt
	systemPrompt := buildSystemPrompt(aiCtx)

	// Build request
	apiReq := anthropicRequest{
		Model:     s.cfg.AI.Model,
		MaxTokens: 4096,
		System:    systemPrompt,
		Messages:  apiMessages,
		Stream:    true,
	}

	reqBody, err := json.Marshal(apiReq)
	if err != nil {
		return nil, nil, fmt.Errorf("构建请求失败: %w", err)
	}

	httpReq, err := http.NewRequest("POST", s.cfg.AI.BaseURL+"/v1/messages", bytes.NewReader(reqBody))
	if err != nil {
		return nil, nil, fmt.Errorf("创建请求失败: %w", err)
	}

	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("x-api-key", s.cfg.AI.APIKey)
	httpReq.Header.Set("anthropic-version", "2023-06-01")

	resp, err := s.httpClient.Do(httpReq)
	if err != nil {
		return nil, nil, fmt.Errorf("发送请求失败: %w", err)
	}

	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		resp.Body.Close()
		return nil, nil, fmt.Errorf("API 返回错误 %d: %s", resp.StatusCode, string(body))
	}

	tokenChan := make(chan string, 100)
	errChan := make(chan error, 1)

	go func() {
		defer resp.Body.Close()
		defer close(tokenChan)
		defer close(errChan)

		var fullResponse strings.Builder
		scanner := bufio.NewScanner(resp.Body)
		// Increase buffer for large SSE lines
		scanner.Buffer(make([]byte, 64*1024), 1024*1024)

		for scanner.Scan() {
			line := scanner.Text()

			if !strings.HasPrefix(line, "data: ") {
				continue
			}

			data := strings.TrimPrefix(line, "data: ")

			var event anthropicStreamEvent
			if err := json.Unmarshal([]byte(data), &event); err != nil {
				continue
			}

			if event.Type == "content_block_delta" && event.Delta != nil {
				tokenChan <- event.Delta.Text
				fullResponse.WriteString(event.Delta.Text)
			}
		}

		if err := scanner.Err(); err != nil {
			errChan <- fmt.Errorf("读取流失败: %w", err)
			return
		}

		// Save assistant response
		assistantMsg := models.AIMessage{
			ConversationID: conversationID,
			Role:          "assistant",
			Content:       fullResponse.String(),
		}
		database.DB.Create(&assistantMsg)
	}()

	return tokenChan, errChan, nil
}

// GetConversations returns AI conversations for a user.
func GetConversations(userID uint) ([]models.AIConversation, error) {
	var conversations []models.AIConversation
	if err := database.DB.Where("user_id = ?", userID).
		Order("updated_at DESC").
		Find(&conversations).Error; err != nil {
		return nil, err
	}
	return conversations, nil
}

// GetConversationMessages returns messages for a conversation.
func GetConversationMessages(conversationID uint, userID uint) ([]models.AIMessage, error) {
	var messages []models.AIMessage
	if err := database.DB.Where("conversation_id = ?", conversationID).
		Order("created_at ASC").
		Find(&messages).Error; err != nil {
		return nil, err
	}

	// Verify ownership
	var conv models.AIConversation
	if err := database.DB.First(&conv, conversationID).Error; err != nil {
		return nil, err
	}
	if conv.UserID != userID {
		return nil, fmt.Errorf("permission denied")
	}

	return messages, nil
}

// DeleteConversation deletes a conversation and its messages.
func DeleteConversation(conversationID uint, userID uint) error {
	var conv models.AIConversation
	if err := database.DB.First(&conv, conversationID).Error; err != nil {
		return err
	}
	if conv.UserID != userID {
		return fmt.Errorf("permission denied")
	}

	database.DB.Where("conversation_id = ?", conversationID).Delete(&models.AIMessage{})
	database.DB.Delete(&conv)
	return nil
}

// AIChatContext provides the AI with context about the current Ledis connection.
type AIChatContext struct {
	ConnectedServer string
	RecentCommands  []string
	KeyCount        int64
}

func buildSystemPrompt(ctx AIChatContext) string {
	prompt := `你是一个 Redis 专家助手，正在帮助学生学习 Redis 数据库。

你的职责：
1. 解释 Redis 命令的用法、参数和返回值
2. 审查用户执行的 Redis 命令，提出优化建议
3. 回答 Redis 相关概念问题（数据结构、持久化、集群、内存管理等）
4. 帮助调试命令执行中的错误和异常
5. 推荐最佳实践和使用场景

请用中文回答，回答应当简洁专业、易于理解。涉及代码时使用 Markdown 代码块格式。`

	if ctx.ConnectedServer != "" {
		prompt += fmt.Sprintf(`

当前连接状态：
- 服务器: %s
- Key 数量: %d`, ctx.ConnectedServer, ctx.KeyCount)

		if len(ctx.RecentCommands) > 0 {
			prompt += "\n- 最近执行命令:\n"
			for _, cmd := range ctx.RecentCommands {
				prompt += fmt.Sprintf("  > %s\n", cmd)
			}
		}
	}

	return prompt
}
