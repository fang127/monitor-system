# api_gateway

`api_gateway` 是 monitor-system 的 HTTP API 网关服务，使用 Go 和 Gin 实现。它对外提供 JSON HTTP 接口，对内通过 gRPC 调用 C++ Manager 暴露的 `QueryService`。

## 功能

- `GET /health`：服务健康检查
- `GET /api/version`：服务版本信息
- `GET /api/servers/latest`：查询所有服务器最新评分和集群统计
- `GET /api/servers/:server/trend`：查询指定服务器趋势数据
- `GET /api/servers/:server/anomalies`：查询指定服务器异常数据

## 目录结构

```text
api_gateway/
├── cmd/server              # HTTP Server 入口
├── internal/config         # 环境变量配置
├── internal/grpcclient     # C++ Manager QueryService gRPC client
├── internal/handler        # Gin HTTP handlers
├── internal/logger         # 日志初始化
├── internal/response       # JSON 响应封装
├── Makefile
├── go.mod
└── README.md
```

## 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `API_GATEWAY_PORT` | `8080` | HTTP 服务监听端口 |
| `API_GATEWAY_VERSION` | `v0.1.0` | `/api/version` 返回的版本号 |
| `GIN_MODE` | `debug` | Gin 运行模式，支持 `debug`、`release`、`test` |
| `MANAGER_GRPC_ADDR` | `127.0.0.1:50051` | C++ Manager gRPC 地址 |
| `MANAGER_GRPC_TIMEOUT` | `5s` | 调用 Manager 的超时时间 |

## 运行

```bash
cd api_gateway
make run
```

示例：

```bash
MANAGER_GRPC_ADDR=127.0.0.1:50051 API_GATEWAY_PORT=8080 make run
```

## 生成 Go protobuf/gRPC 代码

`query_api.proto` 已补充 `go_package`。如果本机没有 `protoc-gen-go` 或 `protoc-gen-go-grpc`，`make proto` 会自动安装到本地 `.bin/` 目录，并临时加入 PATH。

```bash
cd api_gateway
make proto
```

生成文件输出到：

```text
api_gateway/internal/pb/queryapi/
```

## HTTP 查询参数

### `GET /api/servers/:server/trend`

| 参数 | 说明 |
|------|------|
| `start_time` | 可选，RFC3339 或 Unix 秒，默认最近 1 小时 |
| `end_time` | 可选，RFC3339 或 Unix 秒，默认当前时间 |
| `interval_seconds` | 可选，趋势聚合间隔；`0` 表示不聚合 |

### `GET /api/servers/:server/anomalies`

| 参数 | 说明 |
|------|------|
| `start_time` | 可选，RFC3339 或 Unix 秒，默认最近 1 小时 |
| `end_time` | 可选，RFC3339 或 Unix 秒，默认当前时间 |
| `page` | 可选，默认 `1` |
| `page_size` | 可选，默认 `100` |
| `cpu_threshold` | 可选，CPU 使用率阈值；不传时由 Manager 使用默认值 |
| `mem_threshold` | 可选，内存使用率阈值；不传时由 Manager 使用默认值 |
| `disk_threshold` | 可选，磁盘利用率阈值；不传时由 Manager 使用默认值 |
| `change_rate_threshold` | 可选，变化率阈值；不传时由 Manager 使用默认值 |

## 示例

```bash
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/api/version
curl http://127.0.0.1:8080/api/servers/latest
curl "http://127.0.0.1:8080/api/servers/server-01/trend?interval_seconds=60"
curl "http://127.0.0.1:8080/api/servers/server-01/anomalies?page=1&page_size=50"
```
