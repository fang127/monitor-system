package chat

import (
	"context"
	"errors"
	v1 "monitor-system/agent_service/api/chat/v1"
	"monitor-system/agent_service/internal/ai/agent/plan_execute_replan"
	"monitor-system/agent_service/utility/middleware"

	"github.com/gin-gonic/gin"
)

// AIOps 生成 AI 运维分析报告
func (c *ControllerV1) AIOps(gctx *gin.Context) {
	res, err := c.runAIOps(gctx.Request.Context(), &v1.AIOpsReq{})
	middleware.Respond(gctx, res, err)
}

func (c *ControllerV1) runAIOps(ctx context.Context, req *v1.AIOpsReq) (res *v1.AIOpsRes, err error) {
	_ = req
	// 构建查询指令，指导 AI 进行分析
	query := `
"1. 你是 monitor_system 的 AI 运维分析助手，必须优先使用工具查询当前监控事实。"
"2. 首先调用 query_monitor_cluster_overview 获取集群概览、服务器在线状态、评分和关键指标快照。"
"3. 再调用 query_monitor_anomalies；server_name 留空时表示分析所有服务器的异常记录。"
"4. 对异常或低分服务器，按需调用 query_monitor_performance、query_monitor_trend、query_monitor_detail 查询最近性能、趋势、网络、磁盘、内存、软中断明细。"
"5. 当异常包含 MYSQL_*、服务器低分且疑似数据库相关、或用户问题涉及数据库时，调用 query_monitor_mysql_detail 或 query_monitor_detail(kind=mysql) 查询 MySQL 明细；重点关注 up、connection_used_percent、qps、tps、slow_queries_rate、innodb_row_lock_waits_rate、innodb_buffer_pool_hit_percent、replication_lag_seconds。"
"6. 当异常包含 REDIS_*、服务器低分且疑似缓存相关、或用户问题涉及 Redis 时，调用 query_monitor_redis_detail 或 query_monitor_detail(kind=redis) 查询 Redis 明细；重点关注 up、connection_used_percent、memory_used_percent、commands_per_sec、keyspace_hit_percent、master_last_io_seconds_ago、slowlog_growth。"
"7. 调用 query_internal_docs 搜索内部运维文档，获取相关处理建议；没有文档依据时必须明确说明。"
"8. 涉及当前时间或时间窗口时，先调用 get_current_time，再构造查询参数。"
"9. 不允许编造监控数据，不允许直接访问数据库，不允许使用外部日志系统作为事实来源。"
"10. 最后生成中文 AI 运维分析报告，格式如下：
AI 运维分析报告
---
# 集群健康概览
# 异常与低分服务器清单
# 根因分析
# 处理建议
# 需要人工确认的事项
## 结论
`

	// 构建计划并执行
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
	// 待办：结果后处理，提取结论、建议等结构化信息
	return res, nil

}
