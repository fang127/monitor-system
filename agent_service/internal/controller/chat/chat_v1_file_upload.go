package chat

import (
	"context"
	"errors"
	"fmt"
	v1 "monitor-system/agent_service/api/chat/v1"
	"monitor-system/agent_service/internal/ai/agent/knowledge_index_pipeline"
	loader2 "monitor-system/agent_service/internal/ai/loader"
	"monitor-system/agent_service/utility/client"
	"monitor-system/agent_service/utility/common"
	"monitor-system/agent_service/utility/log_call_back"
	"monitor-system/agent_service/utility/middleware"
	"os"
	"path/filepath"
	"strings"

	"github.com/cloudwego/eino/components/document"
	"github.com/cloudwego/eino/compose"
	"github.com/gin-gonic/gin"
)

// 处理文档上传，并把文档写入 Milvus 知识库

// FileUpload 文档上传接口
func (c *ControllerV1) FileUpload(gctx *gin.Context) {
	res, err := c.runFileUpload(gctx.Request.Context(), gctx)
	middleware.Respond(gctx, res, err)
}

// runFileUpload 处理文件上传的核心逻辑
func (c *ControllerV1) runFileUpload(ctx context.Context, gctx *gin.Context) (res *v1.FileUploadRes, err error) {
	// 从请求中获取上传的文件
	uploadFile, err := gctx.FormFile("file")
	if err != nil {
		return nil, errors.New("请上传文件")
	}

	// 确保保存目录存在
	if err := os.MkdirAll(common.FileDir, 0755); err != nil {
		return nil, fmt.Errorf("创建目录失败: %s: %w", common.FileDir, err)
	}

	// 获取原始文件名
	newFileName := filepath.Base(uploadFile.Filename)

	// 保存文件
	savePath := filepath.Join(common.FileDir, newFileName)
	if err := gctx.SaveUploadedFile(uploadFile, savePath); err != nil {
		return nil, fmt.Errorf("保存文件失败: %w", err)
	}

	// 获取文件信息
	fileInfo, err := os.Stat(savePath)
	if err != nil {
		return nil, fmt.Errorf("获取文件信息失败: %w", err)
	}

	res = &v1.FileUploadRes{
		FileName: newFileName,
		FilePath: savePath,
		FileSize: fileInfo.Size(),
	}
	err = buildIntoIndex(ctx, savePath)
	if err != nil {
		return nil, fmt.Errorf("构建知识库失败: %w", err)
	}
	return res, nil
}

// buildIntoIndex 将上传的文件构建到 Milvus 知识库中
func buildIntoIndex(ctx context.Context, path string) error {
	r, err := knowledge_index_pipeline.BuildKnowledgeIndexing(ctx)
	if err != nil {
		return err
	}
	// 删除biz数据metadata中_source一样的数据
	loader, err := loader2.NewFileLoader(ctx)
	if err != nil {
		return err
	}
	docs, err := loader.Load(ctx, document.Source{URI: path})
	if err != nil {
		return err
	}
	cli, err := client.NewMilvusClient(ctx)
	if err != nil {
		return err
	}
	// 查询所有metadata中_source一样的数据并删除
	expr := fmt.Sprintf(`metadata["_source"] == "%s"`, docs[0].MetaData["_source"])
	queryResult, err := cli.Query(ctx, common.MilvusCollectionName, []string{}, expr, []string{"id"})
	if err != nil {
		return err
	} else if len(queryResult) > 0 {
		// 提取所有需要删除的id
		var idsToDelete []string
		for _, column := range queryResult {
			if column.Name() == "id" {
				// 假设id是字符串类型，如果不是，需要根据实际类型进行转换
				for i := 0; i < column.Len(); i++ {
					id, err := column.GetAsString(i)
					if err == nil {
						idsToDelete = append(idsToDelete, id)
					}
				}
			}
		}
		// 删除这些数据
		if len(idsToDelete) > 0 {
			deleteExpr := fmt.Sprintf(`id in ["%s"]`, strings.Join(idsToDelete, `","`))
			err = cli.Delete(ctx, common.MilvusCollectionName, "", deleteExpr)
			if err != nil {
				fmt.Printf("[warn] delete existing data failed: %v\n", err)
			} else {
				fmt.Printf("[info] deleted %d existing records with _source: %s\n", len(idsToDelete), docs[0].MetaData["_source"])
			}
		}
	}
	// 重新构建
	ids, err := r.Invoke(ctx, document.Source{URI: path}, compose.WithCallbacks(log_call_back.LogCallback(nil)))
	if err != nil {
		return fmt.Errorf("invoke index graph failed: %w", err)
	}
	fmt.Printf("[done] indexing file: %s, len of parts: %d\n", path, len(ids))
	return nil
}
