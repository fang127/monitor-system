import { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';
import { getTrend } from '../api/dashboard';
import { DataTable, type Column } from '../components/DataTable';
import { MetricLineChart } from '../components/MetricLineChart';
import { QueryControls } from '../components/QueryControls';
import { ErrorState, LoadingState } from '../components/SectionState';
import type { AsyncState, PerformanceRecord, QueryTrendResponse, TimeRangeParams } from '../types/api';
import { formatDateTime, formatNumber, formatPercent, formatScore } from '../utils/format';

function serverFromParams(value: string | undefined): string {
  return value ? decodeURIComponent(value) : '';
}

const columns: Column<PerformanceRecord>[] = [
  { key: 'timestamp', title: '时间', render: (row) => formatDateTime(row.timestamp) },
  { key: 'cpu_percent', title: 'CPU', render: (row) => formatPercent(row.cpu_percent) },
  { key: 'cpu_percent_rate', title: 'CPU变化率', render: (row) => formatNumber(row.cpu_percent_rate, 3) },
  { key: 'mem_used_percent', title: '内存', render: (row) => formatPercent(row.mem_used_percent) },
  { key: 'disk_util_percent', title: '磁盘', render: (row) => formatPercent(row.disk_util_percent) },
  { key: 'score', title: '评分', render: (row) => formatScore(row.score) },
];

export function TrendPage() {
  const { server } = useParams();
  const serverName = serverFromParams(server);
  const [intervalSeconds, setIntervalSeconds] = useState(60);
  const [timeRange, setTimeRange] = useState<TimeRangeParams>({});
  const [state, setState] = useState<AsyncState<QueryTrendResponse>>({ data: null, loading: true, error: null });

  useEffect(() => {
    let active = true;
    setState((current) => ({ ...current, loading: true, error: null }));
    getTrend(serverName, { ...timeRange, interval_seconds: intervalSeconds })
      .then((data) => active && setState({ data, loading: false, error: null }))
      .catch((error: Error) => active && setState({ data: null, loading: false, error: error.message }));
    return () => { active = false; };
  }, [intervalSeconds, serverName, timeRange]);

  return (
    <div className="page-stack">
      <header className="page-header">
        <div><span className="eyebrow">GET /api/servers/:server/trend</span><h1>{serverName} 趋势</h1></div>
      </header>
      <QueryControls
        onApply={setTimeRange}
        extra={
          <label>
            聚合秒数
            <input type="number" min={0} value={intervalSeconds} onChange={(event) => setIntervalSeconds(Number(event.target.value))} />
          </label>
        }
      />
      <section className="section-block">
        {state.loading ? <LoadingState title="趋势数据" /> : null}
        {state.error ? <ErrorState title="趋势数据" message={state.error} /> : null}
        {!state.loading && !state.error ? (
          <>
            <MetricLineChart<PerformanceRecord>
              title="趋势图"
              rows={state.data?.records || []}
              series={[
                { key: 'cpu_percent_rate', name: 'CPU变化率' },
                { key: 'mem_used_percent_rate', name: '内存变化率' },
                { key: 'disk_util_percent_rate', name: '磁盘变化率' },
                { key: 'score', name: '评分' },
              ]}
            />
            <DataTable rows={state.data?.records || []} columns={columns} emptyTitle="趋势数据" />
          </>
        ) : null}
      </section>
    </div>
  );
}
