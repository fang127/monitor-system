package model

import (
	"context"
	"errors"
	"testing"
)

func TestUnavailableModelReturnsTypedError(t *testing.T) {
	m := NewOpenAICompatible(Config{})
	_, err := m.Complete(context.Background(), ChatRequest{Messages: []Message{{Role: "user", Content: "hello"}}})
	if !errors.Is(err, ErrUnavailable) {
		t.Fatalf("err = %v, want ErrUnavailable", err)
	}
}

func TestFallbackModelStreamsChunks(t *testing.T) {
	m := Fallback("hello world")
	ch, err := m.Stream(context.Background(), ChatRequest{})
	if err != nil {
		t.Fatal(err)
	}
	var got string
	for chunk := range ch {
		if chunk.Err != nil {
			t.Fatal(chunk.Err)
		}
		got += chunk.Content
	}
	if got != "hello world" {
		t.Fatalf("stream = %q", got)
	}
}
