import { Link } from 'react-router-dom';
import type { ServerScoreSummary } from '../types/api';
import { formatDateTime, formatPercent, formatScore } from '../utils/format';
import { DataTable, type Column } from './DataTable';
import { StatusBadge } from './StatusBadge';

type ServerTableProps = {
  data: ServerScoreSummary[];
  emptyTitle?: string;
};

const columns: Column<ServerScoreSummary>[] = [
  {
    key: 'server_name',
    title: '服务器',
    render: (row) => (
      <div className="server-cell">
        <Link to={`/servers/${encodeURIComponent(row.server_name)}`} title={row.server_name}>
          {row.server_name}
        </Link>
        <span>{formatDateTime(row.last_update)}</span>
      </div>
    ),
  },
  { key: 'status', title: '状态', render: (row) => <StatusBadge status={row.status} /> },
  { key: 'score', title: '评分', render: (row) => <strong>{formatScore(row.score)}</strong> },
  { key: 'cpu_percent', title: 'CPU', render: (row) => formatPercent(row.cpu_percent) },
  { key: 'mem_used_percent', title: '内存', render: (row) => formatPercent(row.mem_used_percent) },
  { key: 'disk_util_percent', title: '磁盘', render: (row) => formatPercent(row.disk_util_percent) },
  { key: 'load_avg_1', title: 'Load 1m', render: (row) => row.load_avg_1.toFixed(2) },
];

export function ServerTable({ data, emptyTitle = '服务器列表' }: ServerTableProps) {
  return <DataTable rows={data} columns={columns} emptyTitle={emptyTitle} emptyMessage="暂无服务器数据" />;
}
