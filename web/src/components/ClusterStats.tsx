import type { ClusterStats } from '../types/api';
import { formatScore } from '../utils/format';
import { EmptyState } from './SectionState';
import { StatCard } from './StatCard';

type ClusterStatsProps = {
  data: ClusterStats | null;
};

export function ClusterStatsPanel({ data }: ClusterStatsProps) {
  if (!data) {
    return <EmptyState title="集群统计" message="暂无集群统计数据" />;
  }

  return (
    <div className="stats-grid">
      <StatCard label="服务器总数" value={data.total_servers} helper="纳管节点" />
      <StatCard label="在线数" value={data.online_servers} tone="success" helper="状态 ONLINE" />
      <StatCard label="离线数" value={data.offline_servers} tone={data.offline_servers > 0 ? 'danger' : 'success'} helper="状态 OFFLINE" />
      <StatCard label="平均分" value={formatScore(data.avg_score)} helper="集群平均" />
      <StatCard label="最高分" value={formatScore(data.max_score)} tone="success" helper={data.best_server || '最佳服务器'} />
      <StatCard label="最低分" value={formatScore(data.min_score)} tone={data.min_score >= 60 ? 'warning' : 'danger'} helper={data.worst_server || '最低服务器'} />
    </div>
  );
}
