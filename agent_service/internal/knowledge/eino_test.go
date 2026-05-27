package knowledge

import (
	"context"
	"testing"

	"github.com/cloudwego/eino/schema"
	"github.com/milvus-io/milvus-sdk-go/v2/entity"
)

func TestLocalStoreProvidesEinoRetrieverFallback(t *testing.T) {
	store := NewLocalStore(t.TempDir())
	store.docs = []Document{{Source: "runbook.md", Content: "mysql replication lag remediation"}}

	retriever := store.Retriever()
	docs, err := retriever.Retrieve(context.Background(), "mysql lag")
	if err != nil {
		t.Fatalf("Retrieve returned error: %v", err)
	}
	if len(docs) != 1 || docs[0].Content != "mysql replication lag remediation" {
		t.Fatalf("docs = %#v", docs)
	}
	if docs[0].MetaData["_source"] != "runbook.md" {
		t.Fatalf("metadata = %#v", docs[0].MetaData)
	}
}

func TestDocumentsFromSchemaDocuments(t *testing.T) {
	docs := DocumentsFromSchema([]*schema.Document{{
		Content: "redis slowlog",
		MetaData: map[string]any{
			"_source": "redis.md",
		},
	}})
	if len(docs) != 1 || docs[0].Source != "redis.md" {
		t.Fatalf("docs = %#v", docs)
	}
}

func TestMilvusVectorBinaryDimensionUsesEmbeddingDimension(t *testing.T) {
	if got := MilvusVectorBinaryDimension(2048); got != 65536 {
		t.Fatalf("MilvusVectorBinaryDimension(2048) = %d, want 65536", got)
	}
	if got := MilvusVectorBinaryDimension(0); got != 65536 {
		t.Fatalf("MilvusVectorBinaryDimension(0) = %d, want default 65536", got)
	}
}

func TestMilvusSchemaCompatibilityChecksFieldsAndDimension(t *testing.T) {
	compatible := entity.NewSchema().WithName("ops_docs").
		WithField(entity.NewField().WithName("id").WithDataType(entity.FieldTypeVarChar).WithIsPrimaryKey(true).WithMaxLength(255)).
		WithField(entity.NewField().WithName("vector").WithDataType(entity.FieldTypeBinaryVector).WithDim(65536)).
		WithField(entity.NewField().WithName("content").WithDataType(entity.FieldTypeVarChar).WithMaxLength(8192)).
		WithField(entity.NewField().WithName("metadata").WithDataType(entity.FieldTypeJSON))
	if err := CheckMilvusOpsDocsSchema(compatible, 2048); err != nil {
		t.Fatalf("compatible schema returned error: %v", err)
	}

	incompatible := entity.NewSchema().WithName("ops_docs").
		WithField(entity.NewField().WithName("id").WithDataType(entity.FieldTypeVarChar).WithIsPrimaryKey(true).WithMaxLength(255)).
		WithField(entity.NewField().WithName("vector").WithDataType(entity.FieldTypeFloatVector).WithDim(2048)).
		WithField(entity.NewField().WithName("content").WithDataType(entity.FieldTypeVarChar).WithMaxLength(8192)).
		WithField(entity.NewField().WithName("metadata").WithDataType(entity.FieldTypeJSON))
	if err := CheckMilvusOpsDocsSchema(incompatible, 2048); err == nil {
		t.Fatalf("incompatible schema returned nil error")
	}
}
