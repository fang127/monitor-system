# 配置说明

`app.env` 是项目统一运行配置入口。`manager`、`api_gateway`、`agent_service`
仍沿用原来的环境变量读取方式，因此不需要修改业务代码。

模块内配置文件继续保留：

- `configs/manager.env`：旧版 manager 示例配置。
- `agent_service/manifest/config/config.yaml`：agent_service 独立运行时的默认配置。
- `web/.env.example`：web 本地开发示例。

启动除 `manager` 和 `worker` 外的容器化模块：

```bash
docker compose --env-file configs/app.env -f deploy/docker-compose.yml up -d
```

启动宿主机上的 `manager` 前可加载同一个配置：

```bash
set -a
source configs/app.env
set +a
```

## 常见启动问题

如果集群监控或服务器页面出现类似下面的错误：

```text
rpc error: code = Unavailable desc = connection error: desc = "transport: Error while dialing: dial tcp 192.168.65.254:50051: connect: connection refused"
```

说明容器内的 `api_gateway` 正在通过 `DOCKER_MANAGER_GRPC_ADDR` 连接宿主机
上的 C++ `manager`，但目标地址的 `50051` 端口没有可用服务。Compose 会把
所有容器放入 `monitor_system_net` 网络，并通过 `manager-host` 别名访问
宿主侧 manager。请先确认：

```bash
set -a
source configs/app.env
set +a
./build/manager/manager 0.0.0.0:50051
```

如果 `manager` 运行在 WSL/Linux 虚拟环境而 Docker Desktop 容器无法通过
默认的 `host-gateway` 访问它，请把 `configs/app.env` 中的
`MANAGER_HOST_GATEWAY` 改成容器可访问的实际 WSL/Linux IP，并保持
`DOCKER_MANAGER_GRPC_ADDR=manager-host:50051`。
