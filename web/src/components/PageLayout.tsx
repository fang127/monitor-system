import { NavLink, Outlet } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";
import { getSelectedClusterId, saveSelectedClusterId } from "../auth/session";
import { useState } from "react";

const navItems = [
  { to: "/", label: "Dashboard" },
  { to: "/servers", label: "服务器" },
  { to: "/ai-ops", label: "AI 运维" },
  { to: "/system", label: "系统" },
];

export function PageLayout() {
  const { session, user, logout, switchTeam } = useAuth();
  const [clusterId, setClusterId] = useState(getSelectedClusterId());
  const [switching, setSwitching] = useState(false);
  const visibleNavItems =
    user?.role === "admin"
      ? [...navItems, { to: "/users", label: "用户管理" }]
      : navItems;

  async function handleTeamChange(value: string) {
    const [tenantId, teamId] = value.split("::");
    if (!tenantId || !teamId) {
      return;
    }
    setSwitching(true);
    try {
      await switchTeam(tenantId, teamId);
    } finally {
      setSwitching(false);
    }
  }

  function handleClusterChange(value: string) {
    setClusterId(value);
    saveSelectedClusterId(value);
  }

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand-block">
          <span className="brand-mark">MS</span>
          <div>
            <strong>Monitor System</strong>
            <span>API Gateway Console</span>
          </div>
        </div>
        <nav className="nav-list" aria-label="主导航">
          {visibleNavItems.map((item) => (
            <NavLink
              key={item.to}
              to={item.to}
              end={item.to === "/"}
              className={({ isActive }) =>
                `nav-link ${isActive ? "nav-link-active" : ""}`
              }
            >
              {item.label}
            </NavLink>
          ))}
        </nav>
        <div className="sidebar-user">
          <span>{user?.username}</span>
          <strong>{user?.role}</strong>
          {session?.currentScope ? (
            <span>
              {session.currentScope.tenant_name}/
              {session.currentScope.team_name}
            </span>
          ) : null}
          {session?.teams && session.teams.length > 1 ? (
            <select
              value={
                session.currentScope
                  ? `${session.currentScope.tenant_id}::${session.currentScope.team_id}`
                  : ""
              }
              onChange={(event) => handleTeamChange(event.target.value)}
              disabled={switching}
            >
              {session.teams.map((team) => (
                <option
                  key={`${team.tenant_id}:${team.team_id}`}
                  value={`${team.tenant_id}::${team.team_id}`}
                >
                  {team.tenant_name}/{team.team_name}
                </option>
              ))}
            </select>
          ) : null}
          <input
            value={clusterId}
            onChange={(event) => handleClusterChange(event.target.value)}
            placeholder="集群 ID"
          />
          <button type="button" onClick={logout}>
            退出登录
          </button>
        </div>
      </aside>
      <main className="content-shell">
        <Outlet />
      </main>
    </div>
  );
}
