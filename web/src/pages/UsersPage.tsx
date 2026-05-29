import { FormEvent, useState } from "react";
import { createUser } from "../api/auth";
import type { AuthRole, AuthUser } from "../auth/session";

export function UsersPage() {
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [role, setRole] = useState<AuthRole>("user");
  const [created, setCreated] = useState<AuthUser | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setError(null);
    setCreated(null);
    setSubmitting(true);
    try {
      const user = await createUser({ username, password, role });
      setCreated(user);
      setUsername("");
      setPassword("");
      setRole("user");
    } catch (err) {
      setError(err instanceof Error ? err.message : "创建用户失败");
    } finally {
      setSubmitting(false);
    }
  }

  return (
    <div className="page-stack">
      <header className="page-header">
        <div>
          <span className="eyebrow">POST /api/users</span>
          <h1>用户管理</h1>
        </div>
      </header>
      <section className="section-block">
        <form className="auth-form user-form" onSubmit={handleSubmit}>
          <label>
            用户名
            <input
              value={username}
              onChange={(event) => setUsername(event.target.value)}
            />
          </label>
          <label>
            初始密码
            <input
              type="password"
              value={password}
              onChange={(event) => setPassword(event.target.value)}
            />
          </label>
          <label>
            角色
            <select
              value={role}
              onChange={(event) => setRole(event.target.value as AuthRole)}
            >
              <option value="user">user</option>
              <option value="admin">admin</option>
            </select>
          </label>
          {error ? <div className="auth-error">{error}</div> : null}
          {created ? (
            <div className="auth-success">
              已创建 {created.username}，角色 {created.role}
            </div>
          ) : null}
          <button type="submit" disabled={submitting}>
            {submitting ? "创建中" : "创建用户"}
          </button>
        </form>
      </section>
    </div>
  );
}
