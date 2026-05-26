import axios, { type AxiosRequestConfig } from "axios";

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

type AgentChatStreamHandlers = {
  onToken: (token: string) => void;
  onEvent?: (event: string, data: string) => void;
};

const agentApiBaseUrl = import.meta.env.VITE_AGENT_API_BASE_URL || "/api/agent";

const agentClient = axios.create({
  timeout: 120000,
});

function unwrapAgentEnvelope<T>(payload: AgentEnvelope<T>): T {
  const success =
    payload.code === undefined ? payload.message === "OK" : payload.code === 0;
  if (!success) {
    throw new Error(payload.message || "AI 运维服务暂不可用");
  }
  return payload.data as T;
}

function formatAgentError(error: unknown): Error {
  if (axios.isAxiosError(error)) {
    const data = error.response?.data as
      | Partial<AgentEnvelope<unknown>>
      | undefined;
    return new Error(data?.message || error.message || "AI 运维服务暂不可用");
  }
  if (error instanceof Error) {
    return error;
  }
  return new Error("AI 运维服务暂不可用");
}

async function agentPost<T>(
  url: string,
  data?: unknown,
  config?: AxiosRequestConfig,
): Promise<T> {
  try {
    const response = await agentClient.post<AgentEnvelope<T>>(
      `${agentApiBaseUrl}${url}`,
      data,
      config,
    );
    return unwrapAgentEnvelope(response.data);
  } catch (error) {
    throw formatAgentError(error);
  }
}

export function runAIOpsReport(): Promise<AgentOpsResponse> {
  return agentPost<AgentOpsResponse>("/ai_ops");
}

export function sendAgentChat(params: {
  id: string;
  question: string;
}): Promise<AgentChatResponse> {
  return agentPost<AgentChatResponse>("/chat", {
    Id: params.id,
    Question: params.question,
  });
}

function parseSseBlock(block: string): { event: string; data: string } | null {
  const lines = block.split(/\r?\n/);
  let event = "message";
  const data: string[] = [];

  for (const line of lines) {
    if (line.startsWith("event:")) {
      event = line.slice("event:".length).trim();
    }
    if (line.startsWith("data:")) {
      data.push(line.slice("data:".length).replace(/^ /, ""));
    }
  }

  if (data.length === 0) {
    return null;
  }

  return { event, data: data.join("\n") };
}

export async function sendAgentChatStream(
  params: { id: string; question: string },
  handlers: AgentChatStreamHandlers,
): Promise<void> {
  try {
    const response = await fetch(`${agentApiBaseUrl}/chat_stream`, {
      method: "POST",
      headers: {
        Accept: "text/event-stream",
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        Id: params.id,
        Question: params.question,
      }),
    });

    if (!response.ok) {
      throw new Error(`AI 运维服务暂不可用 (${response.status})`);
    }
    if (!response.body) {
      throw new Error("浏览器不支持流式响应");
    }

    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let buffer = "";

    for (;;) {
      const { done, value } = await reader.read();
      buffer += decoder.decode(value || new Uint8Array(), { stream: !done });
      const blocks = buffer.split(/\r?\n\r?\n/);
      buffer = blocks.pop() || "";

      for (const block of blocks) {
        const parsed = parseSseBlock(block);
        if (!parsed) {
          continue;
        }
        handlers.onEvent?.(parsed.event, parsed.data);
        if (parsed.event === "message") {
          handlers.onToken(parsed.data);
        }
        if (parsed.event === "error") {
          throw new Error(parsed.data || "流式响应失败");
        }
      }

      if (done) {
        break;
      }
    }
  } catch (error) {
    throw formatAgentError(error);
  }
}

export function uploadAgentKnowledge(file: File): Promise<AgentUploadResponse> {
  const form = new FormData();
  form.append("file", file);
  return agentPost<AgentUploadResponse>("/upload", form, {
    headers: {
      "Content-Type": "multipart/form-data",
    },
  });
}
