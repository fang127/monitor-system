package knowledge_index_pipeline

import (
	"context"

	"github.com/cloudwego/eino/components/document"
	"github.com/cloudwego/eino/compose"
)

// BuildKnowledgeIndexing 构建知识索引编排流程
func BuildKnowledgeIndexing(ctx context.Context) (r compose.Runnable[document.Source, []string], err error) {
	const (
		FileLoader       = "FileLoader"
		MarkdownSplitter = "MarkdownSplitter"
		MilvusIndexer    = "MilvusIndexer"
	)
	// 创建编排图
	// 输入为 document.Source，输出为 []string（表示索引结果）
	g := compose.NewGraph[document.Source, []string]()
	// 构建编排流程
	// 1. 文件加载器节点
	fileLoaderKeyOfLoader, err := newLoader(ctx)
	if err != nil {
		return nil, err
	}
	_ = g.AddLoaderNode(FileLoader, fileLoaderKeyOfLoader)
	markdownSplitterKeyOfDocumentTransformer, err := newDocumentTransformer(ctx)
	if err != nil {
		return nil, err
	}
	// 2. Markdown分割器节点
	_ = g.AddDocumentTransformerNode(MarkdownSplitter, markdownSplitterKeyOfDocumentTransformer)
	milvusIndexerKeyOfIndexer, err := newIndexer(ctx)
	if err != nil {
		return nil, err
	}
	// 3. Milvus索引器节点
	_ = g.AddIndexerNode(MilvusIndexer, milvusIndexerKeyOfIndexer)

	// 定义节点之间的依赖关系
	_ = g.AddEdge(compose.START, FileLoader)
	_ = g.AddEdge(MilvusIndexer, compose.END)
	_ = g.AddEdge(FileLoader, MarkdownSplitter)
	_ = g.AddEdge(MarkdownSplitter, MilvusIndexer)
	// 编译编排图
	r, err = g.Compile(ctx, compose.WithGraphName("KnowledgeIndexing"), compose.WithNodeTriggerMode(compose.AnyPredecessor))
	if err != nil {
		return nil, err
	}
	return r, err
}
