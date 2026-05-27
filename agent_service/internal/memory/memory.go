package memory

import "sync"

type Message struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type Store struct {
	enabled bool
	window  int
	mu      sync.RWMutex
	items   map[string][]Message
}

func NewStore(enabled bool, window int) *Store {
	if window <= 0 {
		window = 6
	}
	return &Store{
		enabled: enabled,
		window:  window,
		items:   map[string][]Message{},
	}
}

func (s *Store) Append(session string, msg Message) {
	if s == nil || !s.enabled || session == "" || msg.Content == "" {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	messages := append(s.items[session], msg)
	if len(messages) > s.window {
		messages = messages[len(messages)-s.window:]
	}
	s.items[session] = messages
}

func (s *Store) Get(session string) []Message {
	if s == nil || !s.enabled || session == "" {
		return nil
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	messages := s.items[session]
	copied := make([]Message, len(messages))
	copy(copied, messages)
	return copied
}
