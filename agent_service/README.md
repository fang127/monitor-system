# agent_service

`agent_service` is the AI operations service for `monitor-system`. It exposes the `/api/agent` HTTP API consumed by the web AIOps page, reads monitoring facts through `api_gateway`, and builds chat/report agents with Eino `compose.Graph` pipelines.

## Endpoints

| Method | Path | Description |
| --- | --- | --- |
| `GET` | `/health` | Service health |
| `POST` | `/api/agent/chat` | Normal chat |
| `POST` | `/api/agent/chat_stream` | SSE streaming chat |
| `POST` | `/api/agent/upload` | Upload a knowledge document |
| `POST` | `/api/agent/ai_ops` | Generate an AI operations report |

## Configuration

Environment variables override `manifest/config/config.yaml`.

| Variable | Default | Description |
| --- | --- | --- |
| `AGENT_SERVICE_PORT` | `6872` | HTTP port |
| `API_GATEWAY_BASE_URL` | `http://127.0.0.1:8080` | API gateway base URL |
| `AGENT_DOCS_DIR` | `./docs` | Knowledge document directory |
| `AGENT_MEMORY_DIR` | `./memory` | Reserved memory directory |
| `AGENT_MEMORY_ENABLED` | `true` | Enable per-session memory |
| `AGENT_QUICK_API_KEY` / `AGENT_QUICK_BASE_URL` / `AGENT_QUICK_MODEL` | empty | Chat model config |
| `AGENT_THINK_API_KEY` / `AGENT_THINK_BASE_URL` / `AGENT_THINK_MODEL` | empty | Report model config |
| `AGENT_EMBEDDING_API_KEY` / `AGENT_EMBEDDING_MODEL` | empty / `text-embedding-v4` | Reserved embedding config |

The service starts without model or Milvus credentials. In that mode it returns deterministic fact summaries and clear degraded messages.

## Local Run

```bash
PATH=/usr/local/go/bin:$HOME/go/bin:$PATH go run ./cmd/server
```

## Examples

```bash
curl -X POST http://127.0.0.1:6872/api/agent/chat \
  -H "Content-Type: application/json" \
  -d '{"Id":"demo","Question":"总结当前集群健康状态"}'

curl -X POST http://127.0.0.1:6872/api/agent/upload \
  -F "file=@./docs/README.md"

curl -X POST http://127.0.0.1:6872/api/agent/ai_ops
```

## Verification

```bash
PATH=/usr/local/go/bin:$HOME/go/bin:$PATH GOCACHE=/tmp/monitor-system-go-cache-agent go test ./...
PATH=/usr/local/go/bin:$HOME/go/bin:$PATH go build ./...
```
