# OnCall-Agent Architecture Overview

## 1. Project Positioning

`OnCall-Agent` is a GoFrame-based backend plus a static frontend for:

- Knowledge-aware chat (`/api/chat`, `/api/chat_stream`)
- Knowledge ingestion from uploaded files (`/api/upload`)
- AI Ops alert analysis (`/api/ai_ops`)

Core AI orchestration is implemented with CloudWeGo Eino (Graph + ADK), with Milvus as vector store, and MCP-based external tools for logs.

---

## 2. High-Level Layering

- **Entry and HTTP Layer**
  - `main.go`
  - `api/chat/v1/chat.go`
  - `internal/controller/chat/*.go`
- **AI Orchestration Layer**
  - `internal/ai/agent/chat_pipeline/*`
  - `internal/ai/agent/knowledge_index_pipeline/*`
  - `internal/ai/agent/plan_execute_replan/*`
- **Capability and Integration Layer**
  - `internal/ai/tools/*`
  - `internal/ai/models/open_ai.go`
  - `internal/ai/embedder/embedder.go`
  - `internal/ai/retriever/retriever.go`
  - `internal/ai/indexer/indexer.go`
- **Infrastructure Utilities**
  - `utility/client/client.go`
  - `utility/mem/mem.go`
  - `utility/middleware/middleware.go`
  - `utility/log_call_back/log_call_back.go`
  - `utility/common/common.go`
- **Runtime and Deployment**
  - `manifest/config/config.yaml`
  - `manifest/docker/docker-compose.yml`
- **Frontend**
  - `SuperBizAgentFrontend/index.html`
  - `SuperBizAgentFrontend/app.js`

---

## 3. Backend Boot Flow

1. `main.go` reads `file_dir` from config and sets `common.FileDir`.
2. Registers `/api` group with:
   - `CORSMiddleware`
   - `ResponseMiddleware` (unified response envelope)
3. Binds `chat.NewV1()` controller (implements `api/chat.IChatV1`).
4. Starts server on port `6872`.

---

## 4. Core Business Flows

## 4.1 Chat (non-stream): `/api/chat`

Path:

- `internal/controller/chat/chat_v1_chat.go`
- `internal/ai/agent/chat_pipeline/orchestration.go`

Flow:

1. Build `UserMessage{ID, Query, History}` from request and in-memory history (`utility/mem`).
2. Build chat runner with `BuildChatAgent`.
3. Invoke graph and get `*schema.Message` output.
4. Persist one user/assistant pair into in-memory window.
5. Return `answer`.

Graph (`ChatAgent`) nodes:

- `InputToRag`: query string for retrieval
- `MilvusRetriever`: RAG fetch
- `InputToChat`: chat vars (`content`, `history`, `date`)
- `ChatTemplate`: system prompt + history + user input + retrieved docs
- `ReactAgent`: ReAct execution with tools

## 4.2 ChatStream: `/api/chat_stream`

Path:

- `internal/controller/chat/chat_v1_chat_stream.go`
- `internal/logic/sse/sse.go`

Flow:

1. Create SSE client (`Service.Create`), set text/event-stream headers.
2. Build same `UserMessage` as sync chat.
3. Call `runner.Stream(...)` and forward chunks to SSE `message` events.
4. On EOF, send `done` event.
5. Aggregate full output and append history in defer.

## 4.3 File Upload + Index Build: `/api/upload`

Path:

- `internal/controller/chat/chat_v1_file_upload.go`
- `internal/ai/agent/knowledge_index_pipeline/orchestration.go`

Flow:

1. Save uploaded file into `common.FileDir`.
2. For same `_source`, query existing Milvus ids and delete them first.
3. Run `KnowledgeIndexing` graph:
   - `FileLoader` -> `MarkdownSplitter` -> `MilvusIndexer`
4. Return file metadata.

A standalone batch indexer also exists:

- `internal/ai/cmd/knowledge_cmd/main.go`

It walks `./docs` and rebuilds indexes per markdown file.

## 4.4 AI Ops: `/api/ai_ops`

Path:

- `internal/controller/chat/chat_v1_ai_ops.go`
- `internal/ai/agent/plan_execute_replan/*`

Flow:

1. Controller prepares a fixed Chinese instruction template.
2. Calls `BuildPlanAgent(ctx, query)`.
3. Plan-Execute-Replan pipeline runs:
   - Planner: model for decomposition
   - Executor: tools (MCP logs + prometheus + internal docs + time)
   - Replanner: iterative correction
4. Returns final result and detail events.

---

## 5. Data and State

- **Conversation state**: in-memory map by session id (`utility/mem/mem.go`), max window size = 6 messages.
- **Knowledge state**: Milvus DB `agent`, collection `biz` (`utility/common/common.go`).
- **File source marker**: document metadata `_source` used for dedup/delete-before-reindex.

---

## 6. External Dependencies and Integrations

- **Framework**: GoFrame (`github.com/gogf/gf/v2`)
- **AI orchestration**: Eino + Eino ADK
- **LLM**: OpenAI-compatible endpoints (`internal/ai/models/open_ai.go`)
- **Embedding**: DashScope embedding (`internal/ai/embedder/embedder.go`)
- **Vector DB**: Milvus (`utility/client/client.go`)
- **MCP tools**: Tencent MCP SSE endpoint (`internal/ai/tools/query_log.go`)
- **Frontend transport**: Fetch + SSE (`SuperBizAgentFrontend/app.js`)

Milvus local stack is provided by:

- `manifest/docker/docker-compose.yml` (etcd + minio + milvus + attu)

---

## 7. Configuration Model

Primary config file:

- `manifest/config/config.yaml`

Important keys:

- `ds_think_chat_model.*`
- `ds_quick_chat_model.*`
- `doubao_embedding_model.*`
- `file_dir`
- `mcp_url`

CLI demo commands have separate sample configs in:

- `internal/ai/cmd/chat_cmd/config/config.yaml`
- `internal/ai/cmd/knowledge_cmd/config/config.yaml`

---

## 8. Frontend Architecture

Main file:

- `SuperBizAgentFrontend/app.js`

Key behavior:

- `quick` mode -> calls `/api/chat`
- `stream` mode -> calls `/api/chat_stream` and parses SSE events manually
- file upload via `/api/upload`
- AI Ops button triggers `/api/ai_ops`
- local chat history stored in `localStorage`

---

## 9. Risk Register (Priority)

- **P0 - Secret exposure risk**
  - `manifest/config/config.yaml` contains real API keys and endpoint token-like values.
  - Should be moved to environment variables or private secret manager.

- **P1 - Tool stability risk**
  - Some tools use `log.Fatal` inside tool callbacks (`query_internal_docs.go`, `mysql_crud.go`, `get_current_time.go`), which can terminate process unexpectedly.

- **P1 - Alert tool logic short-circuit**
  - `queryPrometheusAlerts()` in `query_metrics_alerts.go` currently returns immediately with empty result before HTTP logic.

- **P1 - Upload path/file stat mismatch possibility**
  - In upload controller, `os.Stat(savePath)` checks directory path, not explicit saved file path.

- **P2 - Coupling and duplication**
  - Index dedup/delete logic appears both in upload controller and `knowledge_cmd` main; can be refactored into shared service.

- **P2 - In-memory session state**
  - `utility/mem` is process-local; not suitable for multi-instance deployment without external memory store.

---

## 10. Suggested Reading Order (Fast Onboarding)

1. `main.go`
2. `api/chat/v1/chat.go`
3. `internal/controller/chat/chat_new.go`
4. `internal/controller/chat/chat_v1_chat.go`
5. `internal/controller/chat/chat_v1_chat_stream.go`
6. `internal/ai/agent/chat_pipeline/orchestration.go`
7. `internal/controller/chat/chat_v1_file_upload.go`
8. `internal/ai/agent/knowledge_index_pipeline/orchestration.go`
9. `internal/controller/chat/chat_v1_ai_ops.go`
10. `internal/ai/agent/plan_execute_replan/plan_execute_replan.go`
11. `internal/ai/tools/*.go`
12. `utility/client/client.go`
13. `manifest/config/config.yaml`
14. `SuperBizAgentFrontend/app.js`

---

## 11. Architecture Summary

This project is a practical AI Ops assistant with three main capability surfaces:

- RAG-enhanced chat
- markdown-to-vector knowledge ingestion
- tool-driven plan/execute/replan operations

Its current architecture is clear for single-node development and demos, with next-stage hardening mainly needed in secrets management, tool error handling, and distributed session/storage concerns.

