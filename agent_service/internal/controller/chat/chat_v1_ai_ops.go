package chat

import (
	"context"
	"errors"
	"monitor-system/agent_service/api/chat/v1"
	"monitor-system/agent_service/internal/ai/agent/plan_execute_replan"
	"monitor-system/agent_service/utility/middleware"

	"github.com/gin-gonic/gin"
)

func (c *ControllerV1) AIOps(gctx *gin.Context) {
	res, err := c.runAIOps(gctx.Request.Context(), &v1.AIOpsReq{})
	middleware.Respond(gctx, res, err)
}

func (c *ControllerV1) runAIOps(ctx context.Context, req *v1.AIOpsReq) (res *v1.AIOpsRes, err error) {
	_ = req
	query := `
"1. 你是 monitor_system 的 AI 运维分析助手，必须优先使用工具查询当前监控事实。"
"2. 首先调用 query_monitor_cluster_overview 获取集群概览、服务器在线状态、评分和关键指标快照。"
"3. 再调用 query_monitor_anomalies；server_name 留空时表示分析所有服务器的异常记录。"
"4. 对异常或低分服务器，按需调用 query_monitor_performance、query_monitor_trend、query_monitor_detail 查询最近性能、趋势、网络、磁盘、内存、软中断明细。"
"5. 调用 query_internal_docs 搜索内部运维文档，获取相关处理建议；没有文档依据时必须明确说明。"
"6. 涉及当前时间或时间窗口时，先调用 get_current_time，再构造查询参数。"
"7. 不允许编造监控数据，不允许直接访问数据库，不允许使用 Prometheus 或外部日志系统作为事实来源。"
"8. 最后生成中文 AI 运维分析报告，格式如下：
AI 运维分析报告
---
# 集群健康概览
# 异常与低分服务器清单
# 根因分析
# 处理建议
# 需要人工确认的事项
## 结论
`

	resp, detail, err := plan_execute_replan.BuildPlanAgent(ctx, query)
	if err != nil {
		return nil, err
	}
	if resp == "" {
		return nil, errors.New("内部错误")
	}
	res = &v1.AIOpsRes{
		Result: resp,
		Detail: detail,
	}
	return res, nil

}
