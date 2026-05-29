# api_gateway

`api_gateway` 是 monitor-system 的 HTTP API 网关服务，使用 Go 和 Gin 实现。它对外提供 JSON HTTP 接口，对内通过 gRPC 调用 C++ Manager 暴露的 `QueryService`。

## 功能

- `GET /health`：服务健康检查
- `GET /api/version`：服务版本信息
- `POST /api/auth/login`：用户名密码登录，返回 JWT
- `GET /api/auth/me`：返回当前登录用户
- `POST /api/users`：管理员创建用户
- `GET /api/servers/latest`：查询所有服务器最新评分和集群统计
- `GET /api/servers/score-rank`：查询服务器评分排序
- `GET /api/servers/:server/performance`：查询指定服务器历史性能数据
- `GET /api/servers/:server/trend`：查询指定服务器趋势数据
- `GET /api/servers/:server/anomalies`：查询指定服务器异常数据
- `GET /api/servers/:server/net-detail`：查询指定服务器网络明细
- `GET /api/servers/:server/disk-detail`：查询指定服务器磁盘明细
- `GET /api/servers/:server/mem-detail`：查询指定服务器内存明细
- `GET /api/servers/:server/softirq-detail`：查询指定服务器软中断明细

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
| `JWT_SECRET` | `monitor-system-dev-secret` | HS256 JWT 签名密钥，生产环境必须修改 |
| `JWT_ACCESS_TTL` | `24h` | 访问令牌有效期 |
| `MYSQL_HOST` | `127.0.0.1` | 用户表所在 MySQL 地址 |
| `MYSQL_PORT` | `3306` | 用户表所在 MySQL 端口 |
| `MYSQL_USER` | `root` | MySQL 用户名 |
| `MYSQL_PASSWORD` | 空 | MySQL 密码 |
| `MYSQL_DATABASE` | `monitor-system` | MySQL 数据库 |
| `ADMIN_USERNAME` | 空 | 用户表为空时引导创建的管理员用户名 |
| `ADMIN_PASSWORD` | 空 | 用户表为空时引导创建的管理员密码 |

## 鉴权

除 `/health`、`/api/version` 和 `/api/auth/login` 外，所有 HTTP API 都要求携带 JWT：

```bash
curl -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"your-password"}'

curl http://127.0.0.1:8080/api/servers/latest \
  -H "Authorization: Bearer <access_token>"
```

首次启动时，服务会确保 `users` 表存在。如果用户表为空，会读取 `ADMIN_USERNAME` 和 `ADMIN_PASSWORD` 创建第一个 `admin` 用户。后续可由 `admin` 调用 `POST /api/users` 创建 `admin` 或 `user` 账号。

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

### 通用分页参数

适用于 `performance`、`anomalies`、`score-rank` 和各类 `*-detail` 接口。

| 参数 | 说明 |
|------|------|
| `page` | 可选，默认 `1` |
| `page_size` | 可选，默认 `100` |

### 通用时间参数

适用于 `performance`、`trend`、`anomalies` 和各类 `*-detail` 接口。

| 参数 | 说明 |
|------|------|
| `start_time` | 可选，RFC3339 或 Unix 秒，默认最近 1 小时 |
| `end_time` | 可选，RFC3339 或 Unix 秒，默认当前时间 |

### `GET /api/servers/score-rank`

| 参数 | 说明 |
|------|------|
| `order` | 可选，`desc` 或 `asc`，默认 `desc` |
| `page` | 可选，默认 `1` |
| `page_size` | 可选，默认 `100` |

### `GET /api/servers/:server/performance`

支持通用时间参数和通用分页参数。

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

### `GET /api/servers/:server/{net,disk,mem,softirq}-detail`

支持通用时间参数和通用分页参数。

## 示例

```bash
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/api/version
curl http://127.0.0.1:8080/api/servers/latest -H "Authorization: Bearer <access_token>"
curl "http://127.0.0.1:8080/api/servers/score-rank?order=desc&page=1&page_size=20" -H "Authorization: Bearer <access_token>"
curl "http://127.0.0.1:8080/api/servers/server-01/performance?page=1&page_size=100"
curl "http://127.0.0.1:8080/api/servers/server-01/trend?interval_seconds=60"
curl "http://127.0.0.1:8080/api/servers/server-01/anomalies?page=1&page_size=50"
curl "http://127.0.0.1:8080/api/servers/server-01/net-detail?page=1&page_size=50"
```
