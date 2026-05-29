export type AuthRole = "admin" | "user";

export type AuthUser = {
  id: number;
  username: string;
  role: AuthRole;
  status?: string;
};

export type AuthSession = {
  accessToken: string;
  expiresAt: string;
  user: AuthUser;
};

const storageKey = "monitor-system-auth";

function parseSession(value: string | null): AuthSession | null {
  if (!value) {
    return null;
  }
  try {
    const parsed = JSON.parse(value) as Partial<AuthSession>;
    if (!parsed.accessToken || !parsed.expiresAt || !parsed.user) {
      return null;
    }
    if (new Date(parsed.expiresAt).getTime() <= Date.now()) {
      localStorage.removeItem(storageKey);
      return null;
    }
    return parsed as AuthSession;
  } catch {
    localStorage.removeItem(storageKey);
    return null;
  }
}

export function getStoredSession(): AuthSession | null {
  return parseSession(localStorage.getItem(storageKey));
}

export function getAuthToken(): string | null {
  return getStoredSession()?.accessToken || null;
}

export function saveAuthSession(session: AuthSession) {
  localStorage.setItem(storageKey, JSON.stringify(session));
}

export function clearAuthSession() {
  localStorage.removeItem(storageKey);
}

export function redirectToLoginOnUnauthorized() {
  clearAuthSession();
  if (window.location.pathname !== "/login") {
    window.location.assign("/login");
  }
}
