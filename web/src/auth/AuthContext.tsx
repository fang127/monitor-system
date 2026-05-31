import {
  createContext,
  type ReactNode,
  useCallback,
  useContext,
  useMemo,
  useState,
} from "react";
import {
  login as loginRequest,
  switchTeam as switchTeamRequest,
} from "../api/auth";
import {
  clearAuthSession,
  getStoredSession,
  saveAuthSession,
  type AuthSession,
  type AuthUser,
} from "./session";

type AuthContextValue = {
  session: AuthSession | null;
  user: AuthUser | null;
  login: (
    username: string,
    password: string,
    tenantId?: string,
    teamId?: string,
  ) => Promise<void>;
  switchTeam: (tenantId: string, teamId: string) => Promise<void>;
  logout: () => void;
};

const AuthContext = createContext<AuthContextValue | null>(null);

export function AuthProvider({ children }: { children: ReactNode }) {
  const [session, setSession] = useState<AuthSession | null>(() =>
    getStoredSession(),
  );

  const login = useCallback(
    async (
      username: string,
      password: string,
      tenantId?: string,
      teamId?: string,
    ) => {
      const nextSession = await loginRequest({
        username,
        password,
        tenant_id: tenantId || undefined,
        team_id: teamId || undefined,
      });
      saveAuthSession(nextSession);
      setSession(nextSession);
    },
    [],
  );

  const switchTeam = useCallback(async (tenantId: string, teamId: string) => {
    const nextSession = await switchTeamRequest({
      tenant_id: tenantId,
      team_id: teamId,
    });
    saveAuthSession(nextSession);
    setSession(nextSession);
  }, []);

  const logout = useCallback(() => {
    clearAuthSession();
    setSession(null);
  }, []);

  const value = useMemo(
    () => ({
      session,
      user: session?.user || null,
      login,
      switchTeam,
      logout,
    }),
    [login, logout, session, switchTeam],
  );

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const value = useContext(AuthContext);
  if (!value) {
    throw new Error("useAuth 必须在 AuthProvider 内使用");
  }
  return value;
}
