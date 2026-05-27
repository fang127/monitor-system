package knowledge

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"unicode"

	einoretriever "github.com/cloudwego/eino/components/retriever"
	"github.com/cloudwego/eino/schema"
)

type Document struct {
	Source  string `json:"source"`
	Content string `json:"content"`
	Score   int    `json:"score"`
}

type IndexResult struct {
	Source  string `json:"source"`
	Chunks  int    `json:"chunks"`
	Warning string `json:"warning,omitempty"`
}

type Store interface {
	IndexFile(ctx context.Context, path string) (IndexResult, error)
	Search(ctx context.Context, query string, limit int) ([]Document, error)
}

type EinoStore interface {
	Store
	Retriever() einoretriever.Retriever
}

type LocalStore struct {
	root string
	mu   sync.RWMutex
	docs []Document
}

func NewLocalStore(root string) *LocalStore {
	if root == "" {
		root = "./docs"
	}
	return &LocalStore{root: root}
}

func (s *LocalStore) IndexFile(ctx context.Context, path string) (IndexResult, error) {
	_ = ctx
	content, err := os.ReadFile(path)
	if err != nil {
		return IndexResult{}, err
	}
	chunks := splitChunks(string(content), 1200)
	if len(chunks) == 0 {
		chunks = []string{""}
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	filtered := s.docs[:0]
	for _, doc := range s.docs {
		if doc.Source != path {
			filtered = append(filtered, doc)
		}
	}
	for _, chunk := range chunks {
		filtered = append(filtered, Document{Source: path, Content: chunk})
	}
	s.docs = filtered
	return IndexResult{Source: path, Chunks: len(chunks)}, nil
}

func (s *LocalStore) Search(ctx context.Context, query string, limit int) ([]Document, error) {
	_ = ctx
	if limit <= 0 {
		limit = 3
	}
	terms := terms(query)
	s.mu.RLock()
	defer s.mu.RUnlock()
	var matches []Document
	for _, doc := range s.docs {
		score := score(doc.Content, terms)
		if score > 0 {
			copy := doc
			copy.Score = score
			matches = append(matches, copy)
		}
	}
	sort.SliceStable(matches, func(i, j int) bool {
		return matches[i].Score > matches[j].Score
	})
	if len(matches) > limit {
		matches = matches[:limit]
	}
	return matches, nil
}

func (s *LocalStore) Retriever() einoretriever.Retriever {
	return localRetriever{store: s}
}

type localRetriever struct {
	store *LocalStore
}

func (r localRetriever) Retrieve(ctx context.Context, query string, opts ...einoretriever.Option) ([]*schema.Document, error) {
	docs, err := r.store.Search(ctx, query, 3)
	if err != nil {
		return nil, err
	}
	out := make([]*schema.Document, 0, len(docs))
	for _, doc := range docs {
		out = append(out, &schema.Document{
			Content: doc.Content,
			MetaData: map[string]any{
				"_source": doc.Source,
				"score":   doc.Score,
			},
		})
	}
	return out, nil
}

func DocumentsFromSchema(docs []*schema.Document) []Document {
	out := make([]Document, 0, len(docs))
	for _, doc := range docs {
		if doc == nil {
			continue
		}
		source := ""
		if doc.MetaData != nil {
			if value, ok := doc.MetaData["_source"]; ok {
				source = fmt.Sprintf("%v", value)
			}
		}
		out = append(out, Document{Source: source, Content: doc.Content})
	}
	return out
}

func SafeUploadPath(root, filename string) (string, error) {
	if strings.TrimSpace(filename) == "" {
		return "", fmt.Errorf("empty filename")
	}
	base := filepath.Base(filename)
	if base != filename {
		return "", fmt.Errorf("invalid filename %q", filename)
	}
	if err := os.MkdirAll(root, 0755); err != nil {
		return "", err
	}
	return filepath.Join(root, base), nil
}

func splitChunks(text string, size int) []string {
	text = strings.TrimSpace(text)
	if text == "" {
		return nil
	}
	var chunks []string
	for len(text) > size {
		cut := strings.LastIndex(text[:size], "\n")
		if cut < size/2 {
			cut = size
		}
		chunks = append(chunks, strings.TrimSpace(text[:cut]))
		text = strings.TrimSpace(text[cut:])
	}
	if text != "" {
		chunks = append(chunks, text)
	}
	return chunks
}

func terms(query string) []string {
	seen := map[string]bool{}
	var out []string
	for _, part := range strings.FieldsFunc(strings.ToLower(query), func(r rune) bool {
		return !unicode.IsLetter(r) && !unicode.IsDigit(r)
	}) {
		if part != "" && !seen[part] {
			seen[part] = true
			out = append(out, part)
		}
	}
	return out
}

func score(content string, ts []string) int {
	lower := strings.ToLower(content)
	total := 0
	for _, term := range ts {
		if strings.Contains(lower, term) {
			total++
		}
	}
	return total
}
