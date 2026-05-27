package memory

import "testing"

func TestStoreKeepsBoundedMessages(t *testing.T) {
	store := NewStore(true, 4)
	store.Append("s1", Message{Role: "user", Content: "one"})
	store.Append("s1", Message{Role: "assistant", Content: "two"})
	store.Append("s1", Message{Role: "user", Content: "three"})
	store.Append("s1", Message{Role: "assistant", Content: "four"})
	store.Append("s1", Message{Role: "user", Content: "five"})

	got := store.Get("s1")
	if len(got) != 4 {
		t.Fatalf("len(messages) = %d, want 4", len(got))
	}
	if got[0].Content != "two" || got[3].Content != "five" {
		t.Fatalf("unexpected trimmed window: %#v", got)
	}
}

func TestDisabledStoreDoesNotPersist(t *testing.T) {
	store := NewStore(false, 4)
	store.Append("s1", Message{Role: "user", Content: "hello"})
	if got := store.Get("s1"); len(got) != 0 {
		t.Fatalf("disabled store returned %#v", got)
	}
}
