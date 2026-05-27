package model

import (
	"context"
	"errors"
	"testing"
)

func TestNewEinoOpenAIModelRequiresCompleteConfig(t *testing.T) {
	_, err := NewEinoOpenAIModel(context.Background(), Config{})
	if !errors.Is(err, ErrUnavailable) {
		t.Fatalf("err = %v, want ErrUnavailable", err)
	}
}

func TestNewDashScopeEmbedderRequiresAPIKey(t *testing.T) {
	_, err := NewDashScopeEmbedder(context.Background(), EmbeddingConfig{Model: "text-embedding-v4", Dimensions: 2048})
	if !errors.Is(err, ErrUnavailable) {
		t.Fatalf("err = %v, want ErrUnavailable", err)
	}
}
