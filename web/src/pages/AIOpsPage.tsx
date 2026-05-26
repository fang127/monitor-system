import {
  FormEvent,
  KeyboardEvent,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import {
  runAIOpsReport,
  sendAgentChat,
  sendAgentChatStream,
  uploadAgentKnowledge,
  type AgentUploadResponse,
} from "../api/agent";

type ChatMessage = {
  id: string;
  role: "user" | "assistant";
  content: string;
  label?: string;
  detail?: string[];
};

type AsyncTextState = {
  loading: boolean;
  error: string | null;
};

type ResponseMode = "stream" | "normal";

function createSessionId() {
  return `web-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}

function normalizeReport(raw: string): string {
  if (!raw) {
    return "";
  }
  try {
    const parsed = JSON.parse(raw) as { response?: string };
    return parsed.response || raw;
  } catch {
    return raw;
  }
}

function formatFileSize(bytes: number) {
  if (!bytes) {
    return "0 B";
  }
  const units = ["B", "KB", "MB", "GB"];
  const index = Math.min(
    Math.floor(Math.log(bytes) / Math.log(1024)),
    units.length - 1,
  );
  return `${(bytes / 1024 ** index).toFixed(index === 0 ? 0 : 1)} ${units[index]}`;
}

function AttachmentIcon() {
  return (
    <svg aria-hidden="true" viewBox="0 0 24 24">
      <path d="M10.7 17.8 17.9 10.6a3.3 3.3 0 0 0-4.7-4.7l-8 8a4.7 4.7 0 0 0 6.6 6.6l8.1-8.1" />
      <path d="m8.9 15.6 7-7" />
    </svg>
  );
}

function SendIcon() {
  return (
    <svg aria-hidden="true" viewBox="0 0 24 24">
      <path d="M12 19V5" />
      <path d="m5 12 7-7 7 7" />
    </svg>
  );
}

export function AIOpsPage() {
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [question, setQuestion] = useState("");
  const [responseMode, setResponseMode] = useState<ResponseMode>("stream");
  const [chatState, setChatState] = useState<AsyncTextState>({
    loading: false,
    error: null,
  });
  const [uploadState, setUploadState] = useState<AsyncTextState>({
    loading: false,
    error: null,
  });
  const [uploadResult, setUploadResult] = useState<AgentUploadResponse | null>(
    null,
  );
  const [sessionId] = useState(createSessionId);
  const messagesEndRef = useRef<HTMLDivElement | null>(null);
  const fileInputRef = useRef<HTMLInputElement | null>(null);

  const isBusy = chatState.loading || uploadState.loading;
  const hasMessages = messages.length > 0;

  const statusText = useMemo(() => {
    if (chatState.loading) {
      return responseMode === "stream" ? "正在流式输出" : "正在生成回复";
    }
    if (uploadState.loading) {
      return "正在上传并写入知识库";
    }
    if (uploadResult) {
      return `已索引 ${uploadResult.fileName}`;
    }
    return "已连接 agent_service";
  }, [chatState.loading, responseMode, uploadResult, uploadState.loading]);

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({
      block: "end",
      behavior: "smooth",
    });
  }, [messages, chatState.loading]);

  function appendAssistantMessage(message: ChatMessage) {
    setMessages((current) => [...current, message]);
  }

  function updateAssistantMessage(
    id: string,
    updater: (message: ChatMessage) => ChatMessage,
  ) {
    setMessages((current) =>
      current.map((message) =>
        message.id === id ? updater(message) : message,
      ),
    );
  }

  async function handleRunReport() {
    if (isBusy) {
      return;
    }
    const userMessage: ChatMessage = {
      id: `${Date.now()}-report-user`,
      role: "user",
      label: "快捷操作",
      content: "生成当前集群的 AI 运维分析报告",
    };
    setMessages((current) => [...current, userMessage]);
    setChatState({ loading: true, error: null });

    try {
      const result = await runAIOpsReport();
      appendAssistantMessage({
        id: `${Date.now()}-report-assistant`,
        role: "assistant",
        label: "AI 运维报告",
        content: normalizeReport(result.result),
        detail: result.detail || [],
      });
      setChatState({ loading: false, error: null });
    } catch (error) {
      setChatState({
        loading: false,
        error: error instanceof Error ? error.message : "生成报告失败",
      });
    }
  }

  async function handleSendChat() {
    const trimmed = question.trim();
    if (!trimmed || isBusy) {
      return;
    }

    const userMessage: ChatMessage = {
      id: `${Date.now()}-user`,
      role: "user",
      content: trimmed,
    };
    const assistantMessageId = `${Date.now()}-assistant`;
    setMessages((current) => [...current, userMessage]);
    setQuestion("");
    setChatState({ loading: true, error: null });

    try {
      if (responseMode === "normal") {
        const result = await sendAgentChat({
          id: sessionId,
          question: trimmed,
        });
        appendAssistantMessage({
          id: assistantMessageId,
          role: "assistant",
          label: "普通输出",
          content: result.answer,
        });
      } else {
        appendAssistantMessage({
          id: assistantMessageId,
          role: "assistant",
          label: "流式输出",
          content: "",
        });
        await sendAgentChatStream(
          { id: sessionId, question: trimmed },
          {
            onToken: (token) => {
              updateAssistantMessage(assistantMessageId, (message) => ({
                ...message,
                content: `${message.content}${token}`,
              }));
            },
          },
        );
      }
      setChatState({ loading: false, error: null });
    } catch (error) {
      setChatState({
        loading: false,
        error: error instanceof Error ? error.message : "发送失败",
      });
      if (responseMode === "stream") {
        updateAssistantMessage(assistantMessageId, (message) => ({
          ...message,
          content: message.content || "流式响应中断，请稍后重试。",
        }));
      }
    }
  }

  async function handleUpload(file: File | null) {
    if (!file || isBusy) {
      return;
    }

    setUploadState({ loading: true, error: null });
    setUploadResult(null);
    try {
      const result = await uploadAgentKnowledge(file);
      setUploadResult(result);
      setMessages((current) => [
        ...current,
        {
          id: `${Date.now()}-upload`,
          role: "assistant",
          label: "知识库",
          content: `文件「${result.fileName}」已上传并写入知识库，大小 ${formatFileSize(result.fileSize)}。后续问答会检索这份文档。`,
        },
      ]);
      setUploadState({ loading: false, error: null });
    } catch (error) {
      setUploadState({
        loading: false,
        error: error instanceof Error ? error.message : "上传失败",
      });
    } finally {
      if (fileInputRef.current) {
        fileInputRef.current.value = "";
      }
    }
  }

  function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    void handleSendChat();
  }

  function handleComposerKeyDown(event: KeyboardEvent<HTMLTextAreaElement>) {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      void handleSendChat();
    }
  }

  return (
    <div className="aiops-chat-page">
      <div className="aiops-chat-header">
        <div>
          <h1>AI 运维</h1>
          <span>{statusText}</span>
        </div>
        <button
          className="aiops-ghost-action"
          type="button"
          disabled={isBusy}
          onClick={handleRunReport}
        >
          生成报告
        </button>
      </div>

      <main
        className={`aiops-conversation ${hasMessages ? "" : "aiops-conversation-empty"}`}
        aria-live="polite"
      >
        {!hasMessages && (
          <section className="aiops-welcome">
            <div className="aiops-mark">AI</div>
            <h2>今天需要排查什么？</h2>
            <p>
              可以直接询问集群健康、异常根因、服务器趋势，也可以先上传运维文档补充知识库。
            </p>
            <div className="aiops-suggestion-grid">
              <button
                type="button"
                onClick={() =>
                  setQuestion("分析当前最需要关注的服务器，并说明优先级")
                }
              >
                分析重点服务器
              </button>
              <button
                type="button"
                onClick={() =>
                  setQuestion("根据最近异常记录，给出根因分析和处理建议")
                }
              >
                追踪异常根因
              </button>
              <button
                type="button"
                onClick={() =>
                  setQuestion("总结当前集群健康状态，列出人工确认事项")
                }
              >
                汇总健康状态
              </button>
            </div>
          </section>
        )}

        {messages.map((message) => (
          <article
            key={message.id}
            className={`aiops-message aiops-message-${message.role}`}
          >
            <div className="aiops-avatar">
              {message.role === "user" ? "我" : "AI"}
            </div>
            <div className="aiops-message-body">
              {message.label && (
                <span className="aiops-message-label">{message.label}</span>
              )}
              <p>{message.content}</p>
              {message.detail && message.detail.length > 0 && (
                <details className="aiops-run-detail">
                  <summary>查看 {message.detail.length} 条执行记录</summary>
                  <div>
                    {message.detail.map((item, index) => (
                      <pre key={`${index}-${item.slice(0, 20)}`}>{item}</pre>
                    ))}
                  </div>
                </details>
              )}
            </div>
          </article>
        ))}

        {chatState.loading && (
          <div className="aiops-thinking" role="status">
            <span />
            <span />
            <span />
          </div>
        )}
        {(chatState.error || uploadState.error) && (
          <div className="aiops-inline-error" role="alert">
            {chatState.error || uploadState.error}
          </div>
        )}
        <div ref={messagesEndRef} />
      </main>

      <form className="aiops-composer" onSubmit={handleSubmit}>
        {uploadResult && (
          <div className="aiops-attachment-chip">
            <span>{uploadResult.fileName}</span>
            <small>{formatFileSize(uploadResult.fileSize)}</small>
          </div>
        )}
        <textarea
          value={question}
          rows={1}
          placeholder="给 AI 运维发送消息"
          disabled={isBusy}
          onChange={(event) => setQuestion(event.target.value)}
          onKeyDown={handleComposerKeyDown}
        />
        <div className="aiops-composer-toolbar">
          <label className="aiops-icon-button" aria-label="上传文件">
            <input
              ref={fileInputRef}
              type="file"
              accept=".md,.txt,.pdf,.csv,.doc,.docx"
              disabled={isBusy}
              onChange={(event) =>
                void handleUpload(event.target.files?.[0] || null)
              }
            />
            <AttachmentIcon />
          </label>
          <div className="aiops-mode-switch" aria-label="输出模式">
            <button
              type="button"
              className={responseMode === "stream" ? "active" : ""}
              disabled={isBusy}
              onClick={() => setResponseMode("stream")}
            >
              流式
            </button>
            <button
              type="button"
              className={responseMode === "normal" ? "active" : ""}
              disabled={isBusy}
              onClick={() => setResponseMode("normal")}
            >
              普通
            </button>
          </div>
          <button
            className="aiops-send-button"
            type="submit"
            aria-label="发送"
            disabled={isBusy || !question.trim()}
          >
            <SendIcon />
          </button>
        </div>
      </form>
    </div>
  );
}
