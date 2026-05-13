import axios, { type AxiosRequestConfig } from 'axios';

type AgentEnvelope<T> = {
  code?: number;
  message: string;
  data?: T;
};

export type AgentChatResponse = {
  answer: string;
};

export type AgentUploadResponse = {
  fileName: string;
  filePath: string;
  fileSize: number;
};

export type AgentOpsResponse = {
  result: string;
  detail: string[];
};

const agentApiBaseUrl = import.meta.env.VITE_AGENT_API_BASE_URL || '/api/agent';

const agentClient = axios.create({
  timeout: 120000,
});

function unwrapAgentEnvelope<T>(payload: AgentEnvelope<T>): T {
  const success = payload.code === undefined ? payload.message === 'OK' : payload.code === 0;
  if (!success) {
    throw new Error(payload.message || 'AI 运维服务暂不可用');
  }
  return payload.data as T;
}

function formatAgentError(error: unknown): Error {
  if (axios.isAxiosError(error)) {
    const data = error.response?.data as Partial<AgentEnvelope<unknown>> | undefined;
    return new Error(data?.message || error.message || 'AI 运维服务暂不可用');
  }
  if (error instanceof Error) {
    return error;
  }
  return new Error('AI 运维服务暂不可用');
}

async function agentPost<T>(url: string, data?: unknown, config?: AxiosRequestConfig): Promise<T> {
  try {
    const response = await agentClient.post<AgentEnvelope<T>>(`${agentApiBaseUrl}${url}`, data, config);
    return unwrapAgentEnvelope(response.data);
  } catch (error) {
    throw formatAgentError(error);
  }
}

export function runAIOpsReport(): Promise<AgentOpsResponse> {
  return agentPost<AgentOpsResponse>('/ai_ops');
}

export function sendAgentChat(params: { id: string; question: string }): Promise<AgentChatResponse> {
  return agentPost<AgentChatResponse>('/chat', {
    Id: params.id,
    Question: params.question,
  });
}

export function uploadAgentKnowledge(file: File): Promise<AgentUploadResponse> {
  const form = new FormData();
  form.append('file', file);
  return agentPost<AgentUploadResponse>('/upload', form, {
    headers: {
      'Content-Type': 'multipart/form-data',
    },
  });
}
