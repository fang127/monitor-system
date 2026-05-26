import { useEffect, useState } from "react";
import { useParams } from "react-router-dom";
import { getPerformance } from "../api/dashboard";
import { DataTable, type Column } from "../components/DataTable";
import { MetricLineChart } from "../components/MetricLineChart";
import { PaginationControls } from "../components/PaginationControls";
import { QueryControls } from "../components/QueryControls";
import { ErrorState, LoadingState } from "../components/SectionState";
import type {
  AsyncState,
  PerformanceRecord,
  QueryPerformanceResponse,
  TimeRangeParams,
} from "../types/api";
import {
  formatBytesRate,
  formatDateTime,
  formatNumber,
  formatPercent,
  formatScore,
  pickBytesRateUnit,
} from "../utils/format";

function serverFromParams(value: string | undefined): string {
  return value ? decodeURIComponent(value) : "";
}

const columns: Column<PerformanceRecord>[] = [
  {
    key: "timestamp",
    title: "时间",
    render: (row) => formatDateTime(row.timestamp),
  },
  {
    key: "cpu_percent",
    title: "CPU",
    render: (row) => formatPercent(row.cpu_percent),
  },
  {
    key: "mem_used_percent",
    title: "内存",
    render: (row) => formatPercent(row.mem_used_percent),
  },
  {
    key: "disk_util_percent",
    title: "磁盘",
    render: (row) => formatPercent(row.disk_util_percent),
  },
  {
    key: "load_avg_1",
    title: "Load 1m",
    render: (row) => formatNumber(row.load_avg_1, 2),
  },
  {
    key: "send_rate",
    title: "发送速率",
    render: (row) => {
      const unit = pickBytesRateUnit([row.send_rate, row.rcv_rate]);
      return formatBytesRate(row.send_rate, unit);
    },
  },
  {
    key: "rcv_rate",
    title: "接收速率",
    render: (row) => {
      const unit = pickBytesRateUnit([row.send_rate, row.rcv_rate]);
      return formatBytesRate(row.rcv_rate, unit);
    },
  },
  { key: "score", title: "评分", render: (row) => formatScore(row.score) },
];

export function PerformancePage() {
  const { server } = useParams();
  const serverName = serverFromParams(server);
  const [page, setPage] = useState(1);
  const [pageSize, setPageSize] = useState(50);
  const [timeRange, setTimeRange] = useState<TimeRangeParams>({});
  const [state, setState] = useState<AsyncState<QueryPerformanceResponse>>({
    data: null,
    loading: true,
    error: null,
  });

  useEffect(() => {
    let active = true;
    setState((current) => ({ ...current, loading: true, error: null }));
    getPerformance(serverName, { ...timeRange, page, page_size: pageSize })
      .then((data) => active && setState({ data, loading: false, error: null }))
      .catch(
        (error: Error) =>
          active &&
          setState({ data: null, loading: false, error: error.message }),
      );
    return () => {
      active = false;
    };
  }, [page, pageSize, serverName, timeRange]);

  return (
    <div className="page-stack">
      <header className="page-header">
        <div>
          <span className="eyebrow">GET /api/servers/:server/performance</span>
          <h1>{serverName} 历史性能</h1>
        </div>
      </header>
      <QueryControls
        onApply={(params) => {
          setPage(1);
          setTimeRange(params);
        }}
      />
      <section className="section-block">
        {state.loading ? <LoadingState title="历史性能" /> : null}
        {state.error ? (
          <ErrorState title="历史性能" message={state.error} />
        ) : null}
        {!state.loading && !state.error ? (
          <>
            <MetricLineChart<PerformanceRecord>
              title="历史性能趋势"
              rows={state.data?.records || []}
              series={[
                { key: "cpu_percent", name: "CPU" },
                { key: "mem_used_percent", name: "内存" },
                { key: "disk_util_percent", name: "磁盘" },
                { key: "score", name: "评分" },
              ]}
            />
            <DataTable
              rows={state.data?.records || []}
              columns={columns}
              emptyTitle="历史性能"
            />
            <PaginationControls
              page={state.data?.page || page}
              pageSize={state.data?.page_size || pageSize}
              totalCount={state.data?.total_count}
              onPageChange={setPage}
              onPageSizeChange={(next) => {
                setPage(1);
                setPageSize(next);
              }}
            />
          </>
        ) : null}
      </section>
    </div>
  );
}
