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
