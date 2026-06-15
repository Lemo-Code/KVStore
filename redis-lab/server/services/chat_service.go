package services

import (
	"encoding/json"
	"log"
	"sync"
	"time"

	"github.com/gorilla/websocket"

	"redis-lab/server/database"
	"redis-lab/server/models"
)

// WSMessage is the WebSocket message format.
type WSMessage struct {
	Type      string       `json:"type"`
	Room      string       `json:"room,omitempty"`
	Content   string       `json:"content,omitempty"`
	User      *UserInfo    `json:"user,omitempty"`
	Timestamp string       `json:"timestamp,omitempty"`
	Message   string       `json:"message,omitempty"`
	Messages  []WSHistory  `json:"messages,omitempty"`
}

type UserInfo struct {
	ID       uint   `json:"id"`
	Username string `json:"username"`
	Avatar   string `json:"avatar"`
}

type WSHistory struct {
	ID        uint     `json:"id"`
	Content   string   `json:"content"`
	User      UserInfo `json:"user"`
	CreatedAt string   `json:"created_at"`
}

// Client represents a single WebSocket connection.
type Client struct {
	Conn     *websocket.Conn
	UserID   uint
	Username string
	Avatar   string
	Rooms    map[string]bool
	Send     chan []byte
}

// ChatHub manages all WebSocket connections and rooms.
type ChatHub struct {
	clients    map[*Client]bool
	rooms      map[string]map[*Client]bool
	register   chan *Client
	unregister chan *Client
	broadcast  chan *broadcastMsg
	mu         sync.RWMutex
}

type broadcastMsg struct {
	message []byte
	room    string
}

func NewChatHub() *ChatHub {
	return &ChatHub{
		clients:    make(map[*Client]bool),
		rooms:      make(map[string]map[*Client]bool),
		register:   make(chan *Client, 256),
		unregister: make(chan *Client, 256),
		broadcast:  make(chan *broadcastMsg, 256),
	}
}

// Run starts the hub's event loop.
func (h *ChatHub) Run() {
	for {
		select {
		case client := <-h.register:
			h.mu.Lock()
			h.clients[client] = true
			h.mu.Unlock()

		case client := <-h.unregister:
			h.mu.Lock()
			if _, ok := h.clients[client]; ok {
				delete(h.clients, client)
				close(client.Send)
				// Remove from all rooms
				for room := range client.Rooms {
					if h.rooms[room] != nil {
						delete(h.rooms[room], client)
						if len(h.rooms[room]) == 0 {
							delete(h.rooms, room)
						}
					}
				}
			}
			h.mu.Unlock()

		case msg := <-h.broadcast:
			h.mu.RLock()
			if clients, ok := h.rooms[msg.room]; ok {
				for client := range clients {
					select {
					case client.Send <- msg.message:
					default:
						// Client's send buffer is full, skip
					}
				}
			}
			h.mu.RUnlock()
		}
	}
}

// HandleClient manages a single WebSocket client.
func (h *ChatHub) HandleClient(client *Client) {
	h.register <- client

	defer func() {
		// Notify rooms about leaving
		for room := range client.Rooms {
			leaveMsg := WSMessage{
				Type:      "left",
				Room:      room,
				User:      &UserInfo{ID: client.UserID, Username: client.Username, Avatar: client.Avatar},
				Timestamp: time.Now().UTC().Format(time.RFC3339),
			}
			data, _ := json.Marshal(leaveMsg)
			h.broadcast <- &broadcastMsg{message: data, room: room}
		}
		h.unregister <- client
		client.Conn.Close()
	}()

	// Write pump
	go func() {
		for msg := range client.Send {
			if err := client.Conn.WriteMessage(websocket.TextMessage, msg); err != nil {
				return
			}
		}
	}()

	// Read pump
	for {
		_, message, err := client.Conn.ReadMessage()
		if err != nil {
			break
		}

		var msg WSMessage
		if err := json.Unmarshal(message, &msg); err != nil {
			errMsg, _ := json.Marshal(WSMessage{Type: "error", Message: "无效的消息格式"})
			client.Send <- errMsg
			continue
		}

		switch msg.Type {
		case "join":
			h.mu.Lock()
			client.Rooms[msg.Room] = true
			if h.rooms[msg.Room] == nil {
				h.rooms[msg.Room] = make(map[*Client]bool)
			}
			h.rooms[msg.Room][client] = true
			h.mu.Unlock()

			// Notify others in room
			joinMsg, _ := json.Marshal(WSMessage{
				Type:      "joined",
				Room:      msg.Room,
				User:      &UserInfo{ID: client.UserID, Username: client.Username, Avatar: client.Avatar},
				Timestamp: time.Now().UTC().Format(time.RFC3339),
			})
			h.broadcast <- &broadcastMsg{message: joinMsg, room: msg.Room}

			// Send history
			go h.sendHistory(client, msg.Room)

		case "leave":
			h.mu.Lock()
			delete(client.Rooms, msg.Room)
			if h.rooms[msg.Room] != nil {
				delete(h.rooms[msg.Room], client)
			}
			h.mu.Unlock()

			leaveMsg, _ := json.Marshal(WSMessage{
				Type:      "left",
				Room:      msg.Room,
				User:      &UserInfo{ID: client.UserID, Username: client.Username, Avatar: client.Avatar},
				Timestamp: time.Now().UTC().Format(time.RFC3339),
			})
			h.broadcast <- &broadcastMsg{message: leaveMsg, room: msg.Room}

		case "chat":
			if msg.Room == "" || msg.Content == "" {
				continue
			}

			// Find room ID
			var room models.ChatRoom
			if err := database.DB.Where("name = ?", msg.Room).First(&room).Error; err != nil {
				continue
			}

			// Save to database
			chatMsg := models.ChatMessage{
				RoomID:  room.ID,
				UserID:  client.UserID,
				Content: msg.Content,
			}
			database.DB.Create(&chatMsg)

			// Broadcast to room
			msgData, _ := json.Marshal(WSMessage{
				Type:      "message",
				Room:      msg.Room,
				User:      &UserInfo{ID: client.UserID, Username: client.Username, Avatar: client.Avatar},
				Content:   msg.Content,
				Timestamp: time.Now().UTC().Format(time.RFC3339),
			})
			h.broadcast <- &broadcastMsg{message: msgData, room: msg.Room}
		}
	}
}

func (h *ChatHub) sendHistory(client *Client, roomName string) {
	var room models.ChatRoom
	if err := database.DB.Where("name = ?", roomName).First(&room).Error; err != nil {
		return
	}

	var messages []models.ChatMessage
	database.DB.Where("room_id = ?", room.ID).
		Preload("User").
		Order("created_at DESC").
		Limit(50).
		Find(&messages)

	var history []WSHistory
	for i := len(messages) - 1; i >= 0; i-- {
		m := messages[i]
		history = append(history, WSHistory{
			ID:      m.ID,
			Content: m.Content,
			User: UserInfo{
				ID:       m.User.ID,
				Username: m.User.Username,
				Avatar:   m.User.Avatar,
			},
			CreatedAt: m.CreatedAt.UTC().Format(time.RFC3339),
		})
	}

	histMsg, _ := json.Marshal(WSMessage{
		Type:     "history",
		Messages: history,
	})
	client.Send <- histMsg
}

// GetRooms returns all chat rooms.
func GetRooms() ([]models.ChatRoom, error) {
	var rooms []models.ChatRoom
	if err := database.DB.Find(&rooms).Error; err != nil {
		return nil, err
	}
	return rooms, nil
}

// GetRoomMessages returns paginated messages for a room.
func GetRoomMessages(roomID uint, before string, limit int) ([]models.ChatMessage, error) {
	var messages []models.ChatMessage
	query := database.DB.Where("room_id = ?", roomID).Preload("User")

	if before != "" {
		t, err := time.Parse(time.RFC3339, before)
		if err == nil {
			query = query.Where("created_at < ?", t)
		}
	}

	if limit <= 0 || limit > 100 {
		limit = 50
	}

	if err := query.Order("created_at DESC").Limit(limit).Find(&messages).Error; err != nil {
		return nil, err
	}

	// Reverse to chronological order
	for i, j := 0, len(messages)-1; i < j; i, j = i+1, j-1 {
		messages[i], messages[j] = messages[j], messages[i]
	}

	return messages, nil
}

// Chat message types for client logging
var _ = log.Default
