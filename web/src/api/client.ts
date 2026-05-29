import axios, { type AxiosRequestConfig } from "axios";
import {
  getAuthToken,
  redirectToLoginOnUnauthorized,
} from "../auth/session";
import type { ApiEnvelope } from "../types/api";

const apiBaseUrl = import.meta.env.VITE_API_BASE_URL || "/api";

const httpClient = axios.create({
  timeout: 10000,
});

httpClient.interceptors.request.use((config) => {
  const token = getAuthToken();
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

function unwrapEnvelope<T>(payload: ApiEnvelope<T>): T {
  if (payload.code !== 0) {
    throw new Error(payload.message || "请求失败，请稍后重试");
  }

  return payload.data as T;
}

function formatError(error: unknown): Error {
  if (axios.isAxiosError(error)) {
    if (error.response?.status === 401) {
      redirectToLoginOnUnauthorized();
    }
    const data = error.response?.data as
      | Partial<ApiEnvelope<unknown>>
      | undefined;
    return new Error(data?.message || error.message || "请求失败，请稍后重试");
  }

  if (error instanceof Error) {
    return error;
  }

  return new Error("请求失败，请稍后重试");
}

export async function apiGet<T>(
  url: string,
  config?: AxiosRequestConfig,
): Promise<T> {
  try {
    const response = await httpClient.get<ApiEnvelope<T>>(
      `${apiBaseUrl}${url}`,
      config,
    );
    return unwrapEnvelope(response.data);
  } catch (error) {
    throw formatError(error);
  }
}

export async function apiPost<T>(
  url: string,
  data?: unknown,
  config?: AxiosRequestConfig,
): Promise<T> {
  try {
    const response = await httpClient.post<ApiEnvelope<T>>(
      `${apiBaseUrl}${url}`,
      data,
      config,
    );
    return unwrapEnvelope(response.data);
  } catch (error) {
    throw formatError(error);
  }
}

export async function rootGet<T>(
  url: string,
  config?: AxiosRequestConfig,
): Promise<T> {
  try {
    const response = await httpClient.get<ApiEnvelope<T>>(url, config);
    return unwrapEnvelope(response.data);
  } catch (error) {
    throw formatError(error);
  }
}
