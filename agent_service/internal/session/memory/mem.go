package memory

import (
	"sync"

	"github.com/cloudwego/eino/schema"
)

// 简单内存实现，提供非常轻量的进程内会话记忆。

var SimpleMemoryMap = make(map[string]*SimpleMemory)
var mu sync.Mutex // 保护全局 map 的互斥锁

// GetSimpleMemory 获取或创建一个 SimpleMemory 实例，确保每个 ID 对应一个唯一的内存实例。
func GetSimpleMemory(id string) *SimpleMemory {
	mu.Lock()
	defer mu.Unlock()
	// 如果存在就返回，不存在就创建
	if mem, ok := SimpleMemoryMap[id]; ok {
		return mem
	} else {
		newMem := &SimpleMemory{
			ID:            id,
			Messages:      []*schema.Message{},
			MaxWindowSize: 6,
		}
		SimpleMemoryMap[id] = newMem
		return newMem
	}
}

// SimpleMemory 通过一个消息列表来存储对话历史，并且在添加新消息时会自动丢弃旧消息以保持窗口大小不超过 MaxWindowSize。
type SimpleMemory struct {
	ID            string            `json:"id"`       // 唯一标识符，可以是用户ID、会话ID等
	Messages      []*schema.Message `json:"messages"` // 存储对话历史的消息列表
	MaxWindowSize int               // 最大窗口大小，超过这个大小时会丢弃旧消息
	mu            sync.Mutex        // 保护 Messages 切片的并发访问
}

// SetMessages 添加新消息到内存中，并确保消息总数不超过 MaxWindowSize。如果超过了，就丢弃最旧的消息以保持窗口大小。
func (c *SimpleMemory) SetMessages(msg *schema.Message) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.Messages = append(c.Messages, msg)
	// TODO 这里可以考虑对前面的消息进行压缩，或者只保留关键信息，以进一步优化内存使用和性能。
	if len(c.Messages) > c.MaxWindowSize {
		// 确保成对丢弃消息，保持对话配对关系
		// 计算需要丢弃的消息数量（必须是偶数）
		excess := len(c.Messages) - c.MaxWindowSize
		if excess%2 != 0 {
			excess++ // 确保丢弃偶数条消息
		}
		// 丢弃前面的消息，保持对话配对
		c.Messages = c.Messages[excess:]
	}
}

// GetMessages 获取当前内存中的消息列表，返回一个新的切片以避免外部修改原始消息列表。
func (c *SimpleMemory) GetMessages() []*schema.Message {
	c.mu.Lock()
	defer c.mu.Unlock()
	// 注意：这里是浅拷贝
	// 外界修改返回的消息列表不会影响原始消息列表，但如果修改了消息对象本身，原始消息列表中的对象也会被修改。
	messages := make([]*schema.Message, len(c.Messages))
	copy(messages, c.Messages)
	return messages
}
