import { apiGet, apiPost } from "./client";
import type { AuthRole, AuthSession, AuthUser } from "../auth/session";

export type LoginParams = {
  username: string;
  password: string;
};

export type CreateUserParams = {
  username: string;
  password: string;
  role: AuthRole;
};

export function login(params: LoginParams): Promise<AuthSession> {
  return apiPost<AuthSession>("/auth/login", params);
}

export function getCurrentUser(): Promise<AuthUser> {
  return apiGet<AuthUser>("/auth/me");
}

export function createUser(params: CreateUserParams): Promise<AuthUser> {
  return apiPost<AuthUser>("/users", params);
}
