import { useMemo, useState } from 'react';
import { runAIOpsReport, sendAgentChat, uploadAgentKnowledge, type AgentUploadResponse } from '../api/agent';
import { EmptyState, ErrorState, LoadingState } from '../components/SectionState';

type ChatMessage = {
  id: string;
  role: 'user' | 'assistant';
  content: string;
};

type AsyncTextState = {
  loading: boolean;
  error: string | null;
};

function createSessionId() {
  return `web-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}

function normalizeReport(raw: string): string {
  if (!raw) {
    return '';
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
    return '0 B';
  }
  const units = ['B', 'KB', 'MB', 'GB'];
  const index = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), units.length - 1);
  return `${(bytes / 1024 ** index).toFixed(index === 0 ? 0 : 1)} ${units[index]}`;
}

export function AIOpsPage() {
  const [report, setReport] = useState('');
  const [detail, setDetail] = useState<string[]>([]);
  const [reportState, setReportState] = useState<AsyncTextState>({ loading: false, error: null });
  const [chatState, setChatState] = useState<AsyncTextState>({ loading: false, error: null });
  const [uploadState, setUploadState] = useState<AsyncTextState>({ loading: false, error: null });
  const [uploadResult, setUploadResult] = useState<AgentUploadResponse | null>(null);
  const [question, setQuestion] = useState('');
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [sessionId] = useState(createSessionId);

  const latestAssistantMessage = useMemo(
    () => [...messages].reverse().find((message) => message.role === 'assistant'),
    [messages],
  );

  async function handleRunReport() {
    setReportState({ loading: true, error: null });
    setReport('');
    setDetail([]);
    try {
      const result = await runAIOpsReport();
      setReport(normalizeReport(result.result));
      setDetail(result.detail || []);
      setReportState({ loading: false, error: null });
    } catch (error) {
      setReportState({ loading: false, error: error instanceof Error ? error.message : '生成报告失败' });
    }
  }

  async function handleSendChat() {
    const trimmed = question.trim();
    if (!trimmed || chatState.loading) {
      return;
    }
    const userMessage: ChatMessage = {
      id: `${Date.now()}-user`,
      role: 'user',
      content: trimmed,
    };
    setMessages((current) => [...current, userMessage]);
    setQuestion('');
    setChatState({ loading: true, error: null });
    try {
      const result = await sendAgentChat({ id: sessionId, question: trimmed });
      setMessages((current) => [
        ...current,
        {
          id: `${Date.now()}-assistant`,
          role: 'assistant',
          content: result.answer,
        },
      ]);
      setChatState({ loading: false, error: null });
    } catch (error) {
      setChatState({ loading: false, error: error instanceof Error ? error.message : '发送失败' });
    }
  }

  async function handleUpload(file: File | null) {
    if (!file) {
      return;
    }
    setUploadState({ loading: true, error: null });
    setUploadResult(null);
    try {
      const result = await uploadAgentKnowledge(file);
      setUploadResult(result);
      setUploadState({ loading: false, error: null });
    } catch (error) {
      setUploadState({ loading: false, error: error instanceof Error ? error.message : '上传失败' });
    }
  }

  return (
    <div className="page-stack aiops-page">
      <header className="page-header">
        <div>
          <span className="eyebrow">POST /api/agent/ai_ops</span>
          <h1>AI 运维</h1>
        </div>
        <button className="primary-action" type="button" disabled={reportState.loading} onClick={handleRunReport}>
          {reportState.loading ? '生成中' : '生成报告'}
        </button>
      </header>

      <section className="section-block aiops-report-section">
        <div className="section-heading">
          <h2>运维分析报告</h2>
          {detail.length > 0 && <span className="muted-text">{detail.length} 条执行记录</span>}
        </div>
        {reportState.loading && <LoadingState title="正在生成报告" message="正在查询集群概览、异常和内部文档" />}
        {reportState.error && <ErrorState title="报告生成失败" message={reportState.error} />}
        {!reportState.loading && !reportState.error && !report && (
          <EmptyState title="等待生成" message="点击生成报告后会展示最新分析结果" />
        )}
        {!reportState.loading && !reportState.error && report && <pre className="aiops-report">{report}</pre>}
      </section>

      <div className="aiops-grid">
        <section className="section-block">
          <div className="section-heading">
            <h2>知识库</h2>
            {uploadResult && <span className="muted-text">{formatFileSize(uploadResult.fileSize)}</span>}
          </div>
          <label className="file-drop">
            <input
              type="file"
              accept=".md,.txt,.pdf,.csv,.doc,.docx"
              disabled={uploadState.loading}
              onChange={(event) => handleUpload(event.target.files?.[0] || null)}
            />
            <span>{uploadState.loading ? '上传中' : '上传文档'}</span>
          </label>
          {uploadState.error && <ErrorState title="上传失败" message={uploadState.error} />}
          {uploadResult && (
            <div className="upload-result">
              <strong>{uploadResult.fileName}</strong>
              <span>{uploadResult.filePath}</span>
            </div>
          )}
        </section>

        <section className="section-block">
          <div className="section-heading">
            <h2>运维问答</h2>
            {latestAssistantMessage && <span className="muted-text">已响应</span>}
          </div>
          <div className="chat-panel" aria-live="polite">
            {messages.length === 0 && <EmptyState title="暂无对话" message="输入问题后会展示回复" />}
            {messages.map((message) => (
              <div key={message.id} className={`chat-message chat-message-${message.role}`}>
                <span>{message.role === 'user' ? '我' : 'AI'}</span>
                <p>{message.content}</p>
              </div>
            ))}
            {chatState.loading && <LoadingState title="正在分析" message="正在查询监控数据和知识库" />}
            {chatState.error && <ErrorState title="发送失败" message={chatState.error} />}
          </div>
          <div className="aiops-chat-input">
            <textarea
              value={question}
              rows={3}
              placeholder="例如：分析当前最需要关注的服务器"
              onChange={(event) => setQuestion(event.target.value)}
            />
            <button type="button" disabled={chatState.loading || !question.trim()} onClick={handleSendChat}>
              发送
            </button>
          </div>
        </section>
      </div>

      {detail.length > 0 && (
        <section className="section-block">
          <div className="section-heading">
            <h2>执行记录</h2>
          </div>
          <div className="detail-log">
            {detail.map((item, index) => (
              <pre key={`${index}-${item.slice(0, 20)}`}>{item}</pre>
            ))}
          </div>
        </section>
      )}
    </div>
  );
}
