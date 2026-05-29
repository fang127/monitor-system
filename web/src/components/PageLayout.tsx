import { NavLink, Outlet } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";

const navItems = [
  { to: "/", label: "Dashboard" },
  { to: "/servers", label: "服务器" },
  { to: "/ai-ops", label: "AI 运维" },
  { to: "/system", label: "系统" },
];

export function PageLayout() {
  const { user, logout } = useAuth();
  const visibleNavItems =
    user?.role === "admin"
      ? [...navItems, { to: "/users", label: "用户管理" }]
      : navItems;

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
