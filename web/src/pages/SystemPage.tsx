import { useEffect, useState } from "react";
import { getGatewayHealth, getGatewayVersion } from "../api/dashboard";
import { ErrorState, LoadingState } from "../components/SectionState";
import { StatCard } from "../components/StatCard";
import type { AsyncState, GatewayHealth, GatewayVersion } from "../types/api";

type SystemData = {
  health: GatewayHealth;
  version: GatewayVersion;
};

export function SystemPage() {
  const [state, setState] = useState<AsyncState<SystemData>>({
    data: null,
    loading: true,
    error: null,
  });

  useEffect(() => {
    let active = true;
    Promise.all([getGatewayHealth(), getGatewayVersion()])
      .then(
        ([health, version]) =>
          active &&
          setState({ data: { health, version }, loading: false, error: null }),
      )
      .catch(
        (error: Error) =>
          active &&
          setState({ data: null, loading: false, error: error.message }),
      );
    return () => {
      active = false;
    };
  }, []);

  return (
    <div className="page-stack">
      <header className="page-header">
        <div>
          <span className="eyebrow">GET /health + GET /api/version</span>
          <h1>系统信息</h1>
        </div>
      </header>
      <section className="section-block">
        {state.loading ? <LoadingState title="系统信息" /> : null}
        {state.error ? (
          <ErrorState title="系统信息" message={state.error} />
        ) : null}
        {!state.loading && !state.error ? (
          <div className="stats-grid stats-grid-compact">
            <StatCard
              label="健康服务"
              value={state.data?.health.service || "--"}
              helper="/health"
            />
            <StatCard
              label="健康状态"
              value={state.data?.health.status || "--"}
              tone={state.data?.health.status === "ok" ? "success" : "danger"}
            />
            <StatCard
              label="版本服务"
              value={state.data?.version.service || "--"}
              helper="/api/version"
            />
            <StatCard
              label="版本号"
              value={state.data?.version.version || "--"}
            />
          </div>
        ) : null}
      </section>
    </div>
  );
}
