import { NavLink, Outlet } from 'react-router-dom';

const navItems = [
  { to: '/', label: 'Dashboard' },
  { to: '/servers', label: '服务器' },
  { to: '/system', label: '系统' },
];

export function PageLayout() {
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
          {navItems.map((item) => (
            <NavLink
              key={item.to}
              to={item.to}
              end={item.to === '/'}
              className={({ isActive }) => `nav-link ${isActive ? 'nav-link-active' : ''}`}
            >
              {item.label}
            </NavLink>
          ))}
        </nav>
      </aside>
      <main className="content-shell">
        <Outlet />
      </main>
    </div>
  );
}
