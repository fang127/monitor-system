package knowledge

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestLocalStoreIndexesAndSearchesDocuments(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "redis.md")
	if err := os.WriteFile(path, []byte("Redis hit rate is low. Check memory pressure and evictions."), 0644); err != nil {
		t.Fatal(err)
	}

	store := NewLocalStore(dir)
	result, err := store.IndexFile(context.Background(), path)
	if err != nil {
		t.Fatalf("IndexFile returned error: %v", err)
	}
	if result.Chunks == 0 {
		t.Fatal("expected indexed chunks")
	}

	docs, err := store.Search(context.Background(), "redis evictions", 3)
	if err != nil {
		t.Fatalf("Search returned error: %v", err)
	}
	if len(docs) == 0 || docs[0].Source != path {
		t.Fatalf("unexpected docs: %#v", docs)
	}
}

func TestSafeJoinRejectsTraversal(t *testing.T) {
	if _, err := SafeUploadPath(t.TempDir(), "../bad.md"); err == nil {
		t.Fatal("expected traversal error")
	}
}
