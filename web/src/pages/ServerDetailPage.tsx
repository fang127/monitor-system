import { useEffect, useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { getAnomalies, getLatestScores, getPerformance, getTrend } from '../api/dashboard';
import { DataTable, type Column } from '../components/DataTable';
import { HealthGauge } from '../components/HealthGauge';
import { MetricLineChart } from '../components/MetricLineChart';
import { ErrorState, LoadingState } from '../components/SectionState';
import { StatCard } from '../components/StatCard';
import { StatusBadge } from '../components/StatusBadge';
import type {
  AnomalyRecord,
  AsyncState,
  PerformanceRecord,
  QueryAnomalyResponse,
  QueryLatestScoreResponse,
  QueryPerformanceResponse,
  QueryTrendResponse,
  ServerScoreSummary,
} from '../types/api';
import { formatDateTime, formatPercent, formatScore, scoreTone } from '../utils/format';

function serverFromParams(value: string | undefined): string {
  return value ? decodeURIComponent(value) : '';
}

type DetailBundle = {
  latest: QueryLatestScoreResponse;
  performance: QueryPerformanceResponse;
  trend: QueryTrendResponse;
  anomalies: QueryAnomalyResponse;
};

const anomalyColumns: Column<AnomalyRecord>[] = [
  { key: 'timestamp', title: '时间', render: (row) => formatDateTime(row.timestamp) },
  { key: 'anomaly_type', title: '类型' },
  { key: 'severity', title: '级别' },
  { key: 'metric_name', title: '指标' },
  { key: 'value', title: '值', render: (row) => row.value.toFixed(2) },
  { key: 'threshold', title: '阈值', render: (row) => row.threshold.toFixed(2) },
];

function findSummary(latest: QueryLatestScoreResponse | null, server: string): ServerScoreSummary | null {
  return latest?.servers.find((item) => item.server_name === server) || null;
}

export function ServerDetailPage() {
  const { server } = useParams();
  const serverName = serverFromParams(server);
  const [state, setState] = useState<AsyncState<DetailBundle>>({ data: null, loading: true, error: null });

  useEffect(() => {
    if (!serverName) {
      setState({ data: null, loading: false, error: '缺少服务器名称' });
      return undefined;
    }

    let active = true;
    setState((current) => ({ ...current, loading: true, error: null }));

    Promise.all([
      getLatestScores(),
      getPerformance(serverName, { page: 1, page_size: 50 }),
      getTrend(serverName, { interval_seconds: 60 }),
      getAnomalies(serverName, { page: 1, page_size: 20 }),
    ])
      .then(([latest, performance, trend, anomalies]) => {
        if (active) {
          setState({ data: { latest, performance, trend, anomalies }, loading: false, error: null });
        }
      })
      .catch((error: Error) => {
        if (active) {
          setState({ data: null, loading: false, error: error.message });
        }
      });

    return () => {
      active = false;
    };
  }, [serverName]);

  const summary = useMemo(() => findSummary(state.data?.latest || null, serverName), [serverName, state.data]);
  const performanceRows = state.data?.performance.records || [];
  const trendRows = state.data?.trend.records || [];

  return (
    <div className="page-stack">
      <header className="page-header">
        <div>
          <span className="eyebrow">服务器详情</span>
          <h1>{serverName || '未知服务器'}</h1>
        </div>
        <div className="button-group">
          <Link className="button-link" to={`/servers/${encodeURIComponent(serverName)}/performance`}>性能</Link>
          <Link className="button-link" to={`/servers/${encodeURIComponent(serverName)}/trend`}>趋势</Link>
          <Link className="button-link" to={`/servers/${encodeURIComponent(serverName)}/anomalies`}>异常</Link>
        </div>
      </header>

      {state.loading ? <LoadingState title="服务器详情" /> : null}
      {state.error ? <ErrorState title="服务器详情" message={state.error} /> : null}
      {!state.loading && !state.error ? (
        <>
          <section className="section-block">
            <div className="summary-grid">
              <div className="health-card">
                <HealthGauge score={summary?.score ?? null} label="服务器评分" />
                {summary ? <StatusBadge status={summary.status} /> : null}
              </div>
              <div className="stats-grid stats-grid-compact">
                <StatCard label="评分" value={formatScore(summary?.score)} tone={summary ? scoreTone(summary.score) : 'neutral'} />
                <StatCard label="CPU" value={formatPercent(summary?.cpu_percent)} />
                <StatCard label="内存" value={formatPercent(summary?.mem_used_percent)} />
                <StatCard label="磁盘" value={formatPercent(summary?.disk_util_percent)} />
                <StatCard label="Load 1m" value={summary ? summary.load_avg_1.toFixed(2) : '--'} />
                <StatCard label="最后更新" value={formatDateTime(summary?.last_update)} />
              </div>
            </div>
          </section>

          <div className="dashboard-grid">
            <section className="section-block">
              <div className="section-heading"><h2>性能趋势</h2></div>
              <MetricLineChart<PerformanceRecord>
                title="性能趋势"
                rows={trendRows.length > 0 ? trendRows : performanceRows}
                series={[
                  { key: 'cpu_percent', name: 'CPU' },
                  { key: 'mem_used_percent', name: '内存' },
                  { key: 'disk_util_percent', name: '磁盘' },
                  { key: 'score', name: '评分' },
                ]}
              />
            </section>
            <section className="section-block">
              <div className="section-heading"><h2>明细入口</h2></div>
              <div className="detail-link-grid">
                <Link to={`/servers/${encodeURIComponent(serverName)}/details/net`}>网络明细</Link>
                <Link to={`/servers/${encodeURIComponent(serverName)}/details/disk`}>磁盘明细</Link>
                <Link to={`/servers/${encodeURIComponent(serverName)}/details/mem`}>内存明细</Link>
                <Link to={`/servers/${encodeURIComponent(serverName)}/details/softirq`}>软中断明细</Link>
              </div>
            </section>
          </div>

          <section className="section-block">
            <div className="section-heading"><h2>最新异常</h2></div>
            <DataTable rows={state.data?.anomalies.anomalies || []} columns={anomalyColumns} emptyTitle="异常记录" />
          </section>
        </>
      ) : null}
    </div>
  );
}
