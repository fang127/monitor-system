import { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';
import { getAnomalies } from '../api/dashboard';
import { DataTable, type Column } from '../components/DataTable';
import { PaginationControls } from '../components/PaginationControls';
import { QueryControls } from '../components/QueryControls';
import { ErrorState, LoadingState } from '../components/SectionState';
import type { AnomalyRecord, AsyncState, QueryAnomalyResponse, TimeRangeParams } from '../types/api';
import { formatDateTime, formatNumber } from '../utils/format';

function serverFromParams(value: string | undefined): string {
  return value ? decodeURIComponent(value) : '';
}

const columns: Column<AnomalyRecord>[] = [
  { key: 'timestamp', title: '时间', render: (row) => formatDateTime(row.timestamp) },
  { key: 'anomaly_type', title: '类型' },
  { key: 'severity', title: '级别' },
  { key: 'metric_name', title: '指标' },
  { key: 'value', title: '值', render: (row) => formatNumber(row.value, 2) },
  { key: 'threshold', title: '阈值', render: (row) => formatNumber(row.threshold, 2) },
];

export function AnomaliesPage() {
  const { server } = useParams();
  const serverName = serverFromParams(server);
  const [page, setPage] = useState(1);
  const [pageSize, setPageSize] = useState(50);
  const [timeRange, setTimeRange] = useState<TimeRangeParams>({});
  const [cpuThreshold, setCpuThreshold] = useState('');
  const [memThreshold, setMemThreshold] = useState('');
  const [diskThreshold, setDiskThreshold] = useState('');
  const [rateThreshold, setRateThreshold] = useState('');
  const [state, setState] = useState<AsyncState<QueryAnomalyResponse>>({ data: null, loading: true, error: null });

  useEffect(() => {
    let active = true;
    setState((current) => ({ ...current, loading: true, error: null }));
    getAnomalies(serverName, {
      ...timeRange,
      page,
      page_size: pageSize,
      cpu_threshold: cpuThreshold ? Number(cpuThreshold) : undefined,
      mem_threshold: memThreshold ? Number(memThreshold) : undefined,
      disk_threshold: diskThreshold ? Number(diskThreshold) : undefined,
      change_rate_threshold: rateThreshold ? Number(rateThreshold) : undefined,
    })
      .then((data) => active && setState({ data, loading: false, error: null }))
      .catch((error: Error) => active && setState({ data: null, loading: false, error: error.message }));
    return () => { active = false; };
  }, [cpuThreshold, diskThreshold, memThreshold, page, pageSize, rateThreshold, serverName, timeRange]);

  return (
    <div className="page-stack">
      <header className="page-header">
        <div><span className="eyebrow">GET /api/servers/:server/anomalies</span><h1>{serverName} 异常记录</h1></div>
      </header>
      <QueryControls
        onApply={(params) => { setPage(1); setTimeRange(params); }}
        extra={
          <>
            <label>CPU阈值<input value={cpuThreshold} onChange={(event) => setCpuThreshold(event.target.value)} inputMode="decimal" /></label>
            <label>内存阈值<input value={memThreshold} onChange={(event) => setMemThreshold(event.target.value)} inputMode="decimal" /></label>
            <label>磁盘阈值<input value={diskThreshold} onChange={(event) => setDiskThreshold(event.target.value)} inputMode="decimal" /></label>
            <label>变化率阈值<input value={rateThreshold} onChange={(event) => setRateThreshold(event.target.value)} inputMode="decimal" /></label>
          </>
        }
      />
      <section className="section-block">
        {state.loading ? <LoadingState title="异常记录" /> : null}
        {state.error ? <ErrorState title="异常记录" message={state.error} /> : null}
        {!state.loading && !state.error ? (
          <>
            <DataTable rows={state.data?.anomalies || []} columns={columns} emptyTitle="异常记录" />
            <PaginationControls page={state.data?.page || page} pageSize={state.data?.page_size || pageSize} totalCount={state.data?.total_count} onPageChange={setPage} onPageSizeChange={(next) => { setPage(1); setPageSize(next); }} />
          </>
        ) : null}
      </section>
    </div>
  );
}
