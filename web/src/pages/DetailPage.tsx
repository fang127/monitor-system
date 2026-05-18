import { useEffect, useMemo, useState } from 'react';
import { useParams } from 'react-router-dom';
import { getDetail } from '../api/dashboard';
import { DataTable, type Column } from '../components/DataTable';
import { MetricLineChart, type ChartSeries } from '../components/MetricLineChart';
import { PaginationControls } from '../components/PaginationControls';
import { QueryControls } from '../components/QueryControls';
import { ErrorState, LoadingState } from '../components/SectionState';
import type { AsyncState, DetailKind, PagedQueryParams, TimeRangeParams } from '../types/api';
import { formatBytesRate, formatDateTime, formatNumber, formatPercent, pickBytesRateUnit } from '../utils/format';

type MetricRow = Record<string, string | number | boolean | null | undefined> & { timestamp: string };

type DetailState = {
  records: MetricRow[];
  total_count: number;
  page: number;
  page_size: number;
};

type DetailConfig = {
  title: string;
  endpoint: string;
  columns: Column<MetricRow>[];
  chartSeries: ChartSeries<MetricRow>[];
};

const detailKinds: DetailKind[] = ['net', 'disk', 'mem', 'softirq', 'mysql'];

function serverFromParams(value: string | undefined): string {
  return value ? decodeURIComponent(value) : '';
}

function normalizeKind(value: string | undefined): DetailKind {
  return detailKinds.includes(value as DetailKind) ? (value as DetailKind) : 'net';
}

function commonColumns(labelKey: string, labelTitle: string): Column<MetricRow>[] {
  return [
    { key: 'timestamp', title: '时间', render: (row) => formatDateTime(String(row.timestamp)) },
    { key: labelKey, title: labelTitle },
  ];
}

const configs: Record<DetailKind, DetailConfig> = {
  net: {
    title: '网络明细',
    endpoint: 'net-detail',
    columns: [
      ...commonColumns('net_name', '网卡'),
      {
        key: 'rcv_bytes_rate',
        title: '接收速率',
        render: (row) => {
          const unit = pickBytesRateUnit([Number(row.rcv_bytes_rate), Number(row.snd_bytes_rate)]);
          return formatBytesRate(Number(row.rcv_bytes_rate), unit);
        },
      },
      {
        key: 'snd_bytes_rate',
        title: '发送速率',
        render: (row) => {
          const unit = pickBytesRateUnit([Number(row.rcv_bytes_rate), Number(row.snd_bytes_rate)]);
          return formatBytesRate(Number(row.snd_bytes_rate), unit);
        },
      },
      { key: 'rcv_packets_rate', title: '收包速率', render: (row) => formatNumber(Number(row.rcv_packets_rate), 1) },
      { key: 'snd_packets_rate', title: '发包速率', render: (row) => formatNumber(Number(row.snd_packets_rate), 1) },
      { key: 'err_in', title: '入错包' },
      { key: 'drop_in', title: '入丢包' },
    ],
    chartSeries: [
      { key: 'rcv_bytes_rate', name: '接收字节' },
      { key: 'snd_bytes_rate', name: '发送字节' },
    ],
  },
  disk: {
    title: '磁盘明细',
    endpoint: 'disk-detail',
    columns: [
      ...commonColumns('disk_name', '磁盘'),
      { key: 'read_bytes_per_sec', title: '读速率', render: (row) => formatBytesRate(Number(row.read_bytes_per_sec)) },
      { key: 'write_bytes_per_sec', title: '写速率', render: (row) => formatBytesRate(Number(row.write_bytes_per_sec)) },
      { key: 'read_iops', title: '读 IOPS', render: (row) => formatNumber(Number(row.read_iops), 1) },
      { key: 'write_iops', title: '写 IOPS', render: (row) => formatNumber(Number(row.write_iops), 1) },
      { key: 'util_percent', title: '利用率', render: (row) => formatPercent(Number(row.util_percent)) },
    ],
    chartSeries: [
      { key: 'read_bytes_per_sec', name: '读速率' },
      { key: 'write_bytes_per_sec', name: '写速率' },
      { key: 'util_percent', name: '利用率' },
    ],
  },
  mem: {
    title: '内存明细',
    endpoint: 'mem-detail',
    columns: [
      { key: 'timestamp', title: '时间', render: (row) => formatDateTime(String(row.timestamp)) },
      { key: 'total', title: '总量 MB', render: (row) => formatNumber(Number(row.total), 1) },
      { key: 'free', title: '空闲 MB', render: (row) => formatNumber(Number(row.free), 1) },
      { key: 'avail', title: '可用 MB', render: (row) => formatNumber(Number(row.avail), 1) },
      { key: 'cached', title: 'Cached MB', render: (row) => formatNumber(Number(row.cached), 1) },
      { key: 'active', title: 'Active MB', render: (row) => formatNumber(Number(row.active), 1) },
    ],
    chartSeries: [
      { key: 'free', name: 'Free' },
      { key: 'avail', name: 'Available' },
      { key: 'cached', name: 'Cached' },
    ],
  },
  softirq: {
    title: '软中断明细',
    endpoint: 'softirq-detail',
    columns: [
      ...commonColumns('cpu_name', 'CPU'),
      { key: 'net_rx', title: 'NET_RX' },
      { key: 'net_tx', title: 'NET_TX' },
      { key: 'timer', title: 'TIMER' },
      { key: 'sched', title: 'SCHED' },
      { key: 'rcu', title: 'RCU' },
      { key: 'net_rx_rate', title: 'NET_RX变化率', render: (row) => formatNumber(Number(row.net_rx_rate), 2) },
    ],
    chartSeries: [
      { key: 'net_rx_rate', name: 'NET_RX变化率' },
      { key: 'net_tx_rate', name: 'NET_TX变化率' },
      { key: 'timer_rate', name: 'TIMER变化率' },
    ],
  },
  mysql: {
    title: 'MySQL 明细',
    endpoint: 'mysql-detail',
    columns: [
      ...commonColumns('instance', '实例'),
      {
        key: 'up',
        title: '可用性',
        render: (row) => (
          <span className={`status-pill ${row.up ? 'status-online' : 'status-critical'}`}>
            {row.up ? '正常' : '不可用'}
          </span>
        ),
      },
      { key: 'role', title: '角色' },
      {
        key: 'connection_used_percent',
        title: '连接压力',
        render: (row) => formatPercent(Number(row.connection_used_percent)),
      },
      { key: 'qps', title: 'QPS', render: (row) => formatNumber(Number(row.qps), 1) },
      { key: 'tps', title: 'TPS', render: (row) => formatNumber(Number(row.tps), 1) },
      { key: 'slow_queries_rate', title: '慢查询/s', render: (row) => formatNumber(Number(row.slow_queries_rate), 2) },
      {
        key: 'innodb_row_lock_waits_rate',
        title: '锁等待/s',
        render: (row) => formatNumber(Number(row.innodb_row_lock_waits_rate), 2),
      },
      {
        key: 'innodb_buffer_pool_hit_percent',
        title: 'Buffer Pool',
        render: (row) => formatPercent(Number(row.innodb_buffer_pool_hit_percent)),
      },
      {
        key: 'replication_lag_seconds',
        title: '复制延迟',
        render: (row) => row.replication_configured ? `${formatNumber(Number(row.replication_lag_seconds), 1)}s` : '--',
      },
    ],
    chartSeries: [
      { key: 'connection_used_percent', name: '连接压力' },
      { key: 'qps', name: 'QPS' },
      { key: 'tps', name: 'TPS' },
      { key: 'slow_queries_rate', name: '慢查询/s' },
      { key: 'innodb_row_lock_waits_rate', name: '锁等待/s' },
      { key: 'innodb_buffer_pool_hit_percent', name: 'Buffer Pool' },
      { key: 'replication_lag_seconds', name: '复制延迟' },
    ],
  },
};

export function DetailPage() {
  const { server, kind } = useParams();
  const serverName = serverFromParams(server);
  const detailKind = normalizeKind(kind);
  const config = configs[detailKind];
  const [page, setPage] = useState(1);
  const [pageSize, setPageSize] = useState(50);
  const [timeRange, setTimeRange] = useState<TimeRangeParams>({});
  const [state, setState] = useState<AsyncState<DetailState>>({ data: null, loading: true, error: null });

  useEffect(() => {
    let active = true;
    const params: PagedQueryParams = { ...timeRange, page, page_size: pageSize };
    setState((current) => ({ ...current, loading: true, error: null }));
    getDetail(serverName, detailKind, params)
      .then((data) => {
        if (active) {
          setState({
            data: {
              records: data.records as unknown as MetricRow[],
              total_count: data.total_count,
              page: data.page,
              page_size: data.page_size,
            },
            loading: false,
            error: null,
          });
        }
      })
      .catch((error: Error) => active && setState({ data: null, loading: false, error: error.message }));
    return () => { active = false; };
  }, [detailKind, page, pageSize, serverName, timeRange]);

  const rows = useMemo(() => state.data?.records || [], [state.data]);

  return (
    <div className="page-stack">
      <header className="page-header">
        <div><span className="eyebrow">{`GET /api/servers/:server/${config.endpoint}`}</span><h1>{serverName} {config.title}</h1></div>
      </header>
      <QueryControls onApply={(params) => { setPage(1); setTimeRange(params); }} />
      <section className="section-block">
        {state.loading ? <LoadingState title={config.title} /> : null}
        {state.error ? <ErrorState title={config.title} message={state.error} /> : null}
        {!state.loading && !state.error ? (
          <>
            <MetricLineChart<MetricRow> title={`${config.title}趋势`} rows={rows} series={config.chartSeries} />
            <DataTable rows={rows} columns={config.columns} emptyTitle={config.title} />
            <PaginationControls page={state.data?.page || page} pageSize={state.data?.page_size || pageSize} totalCount={state.data?.total_count} onPageChange={setPage} onPageSizeChange={(next) => { setPage(1); setPageSize(next); }} />
          </>
        ) : null}
      </section>
    </div>
  );
}
