import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { getLatestScores } from "../api/dashboard";
import { ClusterStatsPanel } from "../components/ClusterStats";
import { HealthGauge } from "../components/HealthGauge";
import { ErrorState, LoadingState } from "../components/SectionState";
import { ServerTable } from "../components/ServerTable";
import type { AsyncState, QueryLatestScoreResponse } from "../types/api";

function initialState<T>(): AsyncState<T> {
  return { data: null, loading: true, error: null };
}

export function Dashboard() {
  const [latest, setLatest] = useState<AsyncState<QueryLatestScoreResponse>>(
    () => initialState<QueryLatestScoreResponse>(),
  );

  useEffect(() => {
    let active = true;
    getLatestScores()
      .then((data) => {
        if (active) {
          setLatest({ data, loading: false, error: null });
        }
      })
      .catch((error: Error) => {
        if (active) {
          setLatest({ data: null, loading: false, error: error.message });
        }
      });

    return () => {
      active = false;
    };
  }, []);

  return (
    <div className="page-stack">
      <header className="page-header">
        <div>
          <span className="eyebrow">GET /api/servers/latest</span>
          <h1>集群监控 Dashboard</h1>
        </div>
        <Link className="button-link" to="/servers">
          查看评分列表
        </Link>
      </header>

      {latest.loading ? <LoadingState title="Dashboard" /> : null}
      {latest.error ? (
        <ErrorState title="Dashboard" message={latest.error} />
      ) : null}
      {!latest.loading && !latest.error ? (
        <>
          <section className="section-block">
            <div className="section-heading">
              <h2>集群统计</h2>
            </div>
            <ClusterStatsPanel data={latest.data?.cluster_stats || null} />
          </section>

          <div className="dashboard-grid">
            <section className="section-block">
              <div className="section-heading">
                <h2>服务器最新快照</h2>
              </div>
              <ServerTable data={latest.data?.servers || []} />
            </section>
            <section className="section-block">
              <div className="section-heading">
                <h2>集群平均健康分</h2>
              </div>
              <HealthGauge
                score={latest.data?.cluster_stats?.avg_score ?? null}
                label="平均分"
              />
            </section>
          </div>
        </>
      ) : null}
    </div>
  );
}
