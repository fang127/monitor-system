import type { ServerStatus } from '../types/api';

function normalizeStatus(status: ServerStatus | number): 'online' | 'offline' | 'warning' | 'critical' {
  if (status === 0 || String(status).toUpperCase() === 'ONLINE') {
    return 'online';
  }
  if (status === 1 || String(status).toUpperCase() === 'OFFLINE') {
    return 'offline';
  }
  if (String(status).toUpperCase() === 'CRITICAL') {
    return 'critical';
  }
  return 'warning';
}

const statusText = {
  online: '在线',
  offline: '离线',
  warning: '预警',
  critical: '严重',
};

type StatusBadgeProps = {
  status: ServerStatus | number;
};

export function StatusBadge({ status }: StatusBadgeProps) {
  const normalized = normalizeStatus(status);
  return <span className={`status-pill status-${normalized}`}>{statusText[normalized]}</span>;
}
