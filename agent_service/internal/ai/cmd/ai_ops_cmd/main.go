package main

import (
	"context"
	"fmt"
	"monitor-system/agent_service/internal/ai/agent/plan_execute_replan"
)

func main() {
	ctx := context.Background()
	query := `
"1. 你是 monitor_system 的 AI 运维分析助手，首先调用 query_monitor_cluster_overview 获取集群概览。"
"2. 调用 query_monitor_anomalies，server_name 留空以分析所有服务器异常。"
"3. 对异常或低分服务器，按需调用性能、趋势和明细工具补充根因证据。"
"4. 当异常包含 MYSQL_*、服务器低分且疑似数据库相关、或用户问题涉及数据库时，调用 query_monitor_mysql_detail 或 query_monitor_detail(kind=mysql)，重点关注 up、connection_used_percent、qps、tps、slow_queries_rate、innodb_row_lock_waits_rate、innodb_buffer_pool_hit_percent、replication_lag_seconds。"
"5. 调用 query_internal_docs 检索内部运维文档，结合监控事实输出中文报告。"
"6. 不允许编造监控数据，不允许直接访问数据库。"`
	resp, detail, err := plan_execute_replan.BuildPlanAgent(ctx, query)
	if err != nil {
		panic(err)
	}
	fmt.Println("----- Final Response -----")
	fmt.Println(resp)
	fmt.Println("----- Final detail -----")
	fmt.Println(detail)
}
