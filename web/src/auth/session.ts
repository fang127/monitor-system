export type AuthRole = "admin" | "user";

export type AuthUser = {
  id: number;
  username: string;
  role: AuthRole;
  status?: string;
};

export type TeamMembership = {
  tenant_id: string;
  tenant_name: string;
  team_id: string;
  team_name: string;
  member_role: string;
  status: string;
};

export type AuthSession = {
  accessToken: string;
  expiresAt: string;
  user: AuthUser;
  currentScope?: TeamMembership;
  teams?: TeamMembership[];
};

const storageKey = "monitor-system-auth";
const clusterStorageKey = "monitor-system-cluster-id";

function normalizeSession(
  raw: Partial<
    AuthSession & {
      access_token: string;
      expires_at: string;
      current_scope: TeamMembership;
    }
  >,
): Partial<AuthSession> {
  return {
    accessToken: raw.accessToken || raw.access_token,
    expiresAt: raw.expiresAt || raw.expires_at,
    user: raw.user,
    currentScope: raw.currentScope || raw.current_scope,
    teams: raw.teams,
  };
}

function parseSession(value: string | null): AuthSession | null {
  if (!value) {
    return null;
  }
  try {
    const parsed = normalizeSession(JSON.parse(value));
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
  localStorage.setItem(storageKey, JSON.stringify(normalizeSession(session)));
}

export function clearAuthSession() {
  localStorage.removeItem(storageKey);
}

export function getSelectedClusterId(): string {
  return localStorage.getItem(clusterStorageKey) || "";
}

export function saveSelectedClusterId(clusterId: string) {
  const next = clusterId.trim();
  if (next) {
    localStorage.setItem(clusterStorageKey, next);
  } else {
    localStorage.removeItem(clusterStorageKey);
  }
}

export function redirectToLoginOnUnauthorized() {
  clearAuthSession();
  if (window.location.pathname !== "/login") {
    window.location.assign("/login");
  }
}
