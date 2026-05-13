package knowledge_index_pipeline

import (
	"context"
	indexer2 "monitor-system/agent_service/internal/ai/indexer"

	"github.com/cloudwego/eino/components/indexer"
)

func newIndexer(ctx context.Context) (idr indexer.Indexer, err error) {
	return indexer2.NewMilvusIndexer(ctx)
}
