package knowledge

import (
	"context"
	"fmt"
	"path/filepath"
	"strconv"
	"strings"

	einofile "github.com/cloudwego/eino-ext/components/document/loader/file"
	"github.com/cloudwego/eino-ext/components/document/transformer/splitter/markdown"
	indexermilvus "github.com/cloudwego/eino-ext/components/indexer/milvus"
	retrievermilvus "github.com/cloudwego/eino-ext/components/retriever/milvus"
	"github.com/cloudwego/eino/components/document"
	einoembedding "github.com/cloudwego/eino/components/embedding"
	einoretriever "github.com/cloudwego/eino/components/retriever"
	"github.com/google/uuid"
	"github.com/milvus-io/milvus-sdk-go/v2/client"
	"github.com/milvus-io/milvus-sdk-go/v2/entity"
)

const (
	DefaultMilvusDatabase   = "monitor_system_agent"
	DefaultMilvusCollection = "ops_docs"
	defaultEmbeddingDim     = 2048
	milvusIDField           = "id"
	milvusVectorField       = "vector"
	milvusContentField      = "content"
	milvusMetadataField     = "metadata"
)

type MilvusConfig struct {
	Address    string
	Database   string
	Collection string
	DocsDir    string
	Dimension  int
	TopK       int
}

type MilvusStore struct {
	local     *LocalStore
	client    client.Client
	indexer   *indexermilvus.Indexer
	retriever einoretriever.Retriever
	loader    document.Loader
	splitter  document.Transformer
	topK      int
}

func NewMilvusStore(ctx context.Context, cfg MilvusConfig, embedder einoembedding.Embedder) (*MilvusStore, error) {
	if strings.TrimSpace(cfg.Address) == "" {
		return nil, fmt.Errorf("milvus address is empty")
	}
	if embedder == nil {
		return nil, fmt.Errorf("embedding is not configured")
	}
	if cfg.Database == "" {
		cfg.Database = DefaultMilvusDatabase
	}
	if cfg.Collection == "" {
		cfg.Collection = DefaultMilvusCollection
	}
	if cfg.Dimension <= 0 {
		cfg.Dimension = defaultEmbeddingDim
	}
	if cfg.TopK <= 0 {
		cfg.TopK = 3
	}

	cli, err := client.NewClient(ctx, client.Config{Address: cfg.Address})
	if err != nil {
		return nil, err
	}
	if err := ensureMilvusDatabase(ctx, cli, cfg.Database); err != nil {
		_ = cli.Close()
		return nil, err
	}
	if err := ensureOpsDocsCollection(ctx, cli, cfg.Collection, cfg.Dimension); err != nil {
		_ = cli.Close()
		return nil, err
	}

	fields := opsDocsFields(cfg.Dimension)
	idx, err := indexermilvus.NewIndexer(ctx, &indexermilvus.IndexerConfig{
		Client:      cli,
		Collection:  cfg.Collection,
		Description: "monitor_system agent internal operations documents",
		Fields:      fields,
		MetricType:  indexermilvus.HAMMING,
		Embedding:   embedder,
	})
	if err != nil {
		_ = cli.Close()
		return nil, err
	}
	ret, err := retrievermilvus.NewRetriever(ctx, &retrievermilvus.RetrieverConfig{
		Client:       cli,
		Collection:   cfg.Collection,
		VectorField:  milvusVectorField,
		OutputFields: []string{milvusIDField, milvusContentField, milvusMetadataField},
		TopK:         cfg.TopK,
		MetricType:   entity.HAMMING,
		Embedding:    embedder,
	})
	if err != nil {
		_ = cli.Close()
		return nil, err
	}
	loader, err := einofile.NewFileLoader(ctx, &einofile.FileLoaderConfig{UseNameAsID: true})
	if err != nil {
		_ = cli.Close()
		return nil, err
	}
	splitter, err := markdown.NewHeaderSplitter(ctx, &markdown.HeaderConfig{
		Headers: map[string]string{
			"#":   "h1",
			"##":  "h2",
			"###": "h3",
		},
		TrimHeaders: false,
	})
	if err != nil {
		_ = cli.Close()
		return nil, err
	}

	return &MilvusStore{
		local:     NewLocalStore(cfg.DocsDir),
		client:    cli,
		indexer:   idx,
		retriever: ret,
		loader:    loader,
		splitter:  splitter,
		topK:      cfg.TopK,
	}, nil
}

func (s *MilvusStore) Close() error {
	if s == nil || s.client == nil {
		return nil
	}
	return s.client.Close()
}

func (s *MilvusStore) IndexFile(ctx context.Context, path string) (IndexResult, error) {
	localResult, localErr := s.local.IndexFile(ctx, path)
	if localErr != nil {
		return IndexResult{}, localErr
	}
	docs, err := s.loader.Load(ctx, document.Source{URI: path})
	if err != nil {
		localResult.Warning = "Milvus index skipped: " + err.Error()
		return localResult, nil
	}
	chunks, err := s.splitter.Transform(ctx, docs)
	if err != nil {
		localResult.Warning = "Milvus index skipped: " + err.Error()
		return localResult, nil
	}
	if len(chunks) == 0 {
		chunks = docs
	}
	for i, doc := range chunks {
		if doc.ID == "" {
			doc.ID = uuid.NewString()
		} else {
			doc.ID = fmt.Sprintf("%s-%d-%s", filepath.Base(path), i, uuid.NewString())
		}
		if len(doc.Content) > 8000 {
			doc.Content = doc.Content[:8000]
		}
		if doc.MetaData == nil {
			doc.MetaData = map[string]any{}
		}
		doc.MetaData["_source"] = path
		doc.MetaData["file_name"] = filepath.Base(path)
	}
	if _, err := s.indexer.Store(ctx, chunks); err != nil {
		localResult.Warning = "Milvus index skipped: " + err.Error()
		return localResult, nil
	}
	localResult.Chunks = len(chunks)
	return localResult, nil
}

func (s *MilvusStore) Search(ctx context.Context, query string, limit int) ([]Document, error) {
	if limit <= 0 {
		limit = s.topK
	}
	if s.retriever != nil {
		docs, err := s.retriever.Retrieve(ctx, query, einoretriever.WithTopK(limit))
		if err == nil {
			return DocumentsFromSchema(docs), nil
		}
	}
	return s.local.Search(ctx, query, limit)
}

func (s *MilvusStore) Retriever() einoretriever.Retriever {
	if s.retriever != nil {
		return s.retriever
	}
	return s.local.Retriever()
}

func MilvusVectorBinaryDimension(embeddingDim int) int {
	if embeddingDim <= 0 {
		embeddingDim = defaultEmbeddingDim
	}
	return embeddingDim * 32
}

func CheckMilvusOpsDocsSchema(schema *entity.Schema, embeddingDim int) error {
	if schema == nil {
		return fmt.Errorf("schema is nil")
	}
	expected := map[string]entity.FieldType{
		milvusIDField:       entity.FieldTypeVarChar,
		milvusVectorField:   entity.FieldTypeBinaryVector,
		milvusContentField:  entity.FieldTypeVarChar,
		milvusMetadataField: entity.FieldTypeJSON,
	}
	if len(schema.Fields) != len(expected) {
		return fmt.Errorf("field count mismatch: got %d want %d", len(schema.Fields), len(expected))
	}
	for _, field := range schema.Fields {
		wantType, ok := expected[field.Name]
		if !ok {
			return fmt.Errorf("unexpected field %q", field.Name)
		}
		if field.DataType != wantType {
			return fmt.Errorf("field %q type mismatch: got %v want %v", field.Name, field.DataType, wantType)
		}
		if field.Name == milvusVectorField {
			dim, err := strconv.Atoi(field.TypeParams["dim"])
			if err != nil {
				return fmt.Errorf("vector dim parse failed: %w", err)
			}
			if dim != MilvusVectorBinaryDimension(embeddingDim) {
				return fmt.Errorf("vector dim mismatch: got %d want %d", dim, MilvusVectorBinaryDimension(embeddingDim))
			}
		}
	}
	return nil
}

func ensureMilvusDatabase(ctx context.Context, cli client.Client, database string) error {
	dbs, err := cli.ListDatabases(ctx)
	if err != nil {
		return fmt.Errorf("list databases: %w", err)
	}
	for _, db := range dbs {
		if db.Name == database {
			return cli.UsingDatabase(ctx, database)
		}
	}
	if err := cli.CreateDatabase(ctx, database); err != nil {
		return fmt.Errorf("create database %q: %w", database, err)
	}
	return cli.UsingDatabase(ctx, database)
}

func ensureOpsDocsCollection(ctx context.Context, cli client.Client, collection string, embeddingDim int) error {
	exists, err := cli.HasCollection(ctx, collection)
	if err != nil {
		return fmt.Errorf("check collection: %w", err)
	}
	if !exists {
		return nil
	}
	coll, err := cli.DescribeCollection(ctx, collection)
	if err != nil {
		return fmt.Errorf("describe collection: %w", err)
	}
	if err := CheckMilvusOpsDocsSchema(coll.Schema, embeddingDim); err != nil {
		if dropErr := cli.DropCollection(ctx, collection); dropErr != nil {
			return fmt.Errorf("drop incompatible collection after schema check %v: %w", err, dropErr)
		}
	}
	return nil
}

func opsDocsFields(embeddingDim int) []*entity.Field {
	return []*entity.Field{
		entity.NewField().WithName(milvusIDField).WithDataType(entity.FieldTypeVarChar).WithIsPrimaryKey(true).WithMaxLength(255),
		entity.NewField().WithName(milvusVectorField).WithDataType(entity.FieldTypeBinaryVector).WithDim(int64(MilvusVectorBinaryDimension(embeddingDim))),
		entity.NewField().WithName(milvusContentField).WithDataType(entity.FieldTypeVarChar).WithMaxLength(8192),
		entity.NewField().WithName(milvusMetadataField).WithDataType(entity.FieldTypeJSON),
	}
}
