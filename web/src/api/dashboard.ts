import { apiGet, rootGet } from './client';
import type {
  DetailKind,
  DetailResponseMap,
  GatewayHealth,
  GatewayVersion,
  PagedQueryParams,
  QueryAnomalyResponse,
  QueryLatestScoreResponse,
  QueryPerformanceResponse,
  QueryScoreRankResponse,
  QueryTrendResponse,
  SortOrder,
  TimeRangeParams,
} from '../types/api';

function serverPath(server: string): string {
  return encodeURIComponent(server);
}

export function getLatestScores(): Promise<QueryLatestScoreResponse> {
  return apiGet<QueryLatestScoreResponse>('/servers/latest');
}

export function getScoreRank(params: { order?: SortOrder; page?: number; page_size?: number }) {
  return apiGet<QueryScoreRankResponse>('/servers/score-rank', { params });
}

export function getPerformance(server: string, params: PagedQueryParams) {
  return apiGet<QueryPerformanceResponse>(`/servers/${serverPath(server)}/performance`, { params });
}

export function getTrend(
  server: string,
  params: TimeRangeParams & { interval_seconds?: number },
): Promise<QueryTrendResponse> {
  return apiGet<QueryTrendResponse>(`/servers/${serverPath(server)}/trend`, { params });
}

export function getAnomalies(
  server: string,
  params: PagedQueryParams & {
    cpu_threshold?: number;
    mem_threshold?: number;
    disk_threshold?: number;
    change_rate_threshold?: number;
    mysql_connection_threshold?: number;
    mysql_replication_lag_threshold?: number;
    mysql_slow_query_rate_threshold?: number;
    mysql_lock_wait_rate_threshold?: number;
    mysql_buffer_pool_hit_threshold?: number;
    redis_connection_threshold?: number;
    redis_memory_threshold?: number;
    redis_hit_rate_threshold?: number;
    redis_replication_lag_threshold?: number;
    redis_slowlog_growth_threshold?: number;
  },
): Promise<QueryAnomalyResponse> {
  return apiGet<QueryAnomalyResponse>(`/servers/${serverPath(server)}/anomalies`, { params });
}

export function getDetail<K extends DetailKind>(
  server: string,
  kind: K,
  params: PagedQueryParams,
): Promise<DetailResponseMap[K]> {
  const endpoint = kind === 'softirq' ? 'softirq-detail' : `${kind}-detail`;
  return apiGet<DetailResponseMap[K]>(`/servers/${serverPath(server)}/${endpoint}`, { params });
}

export function getGatewayHealth(): Promise<GatewayHealth> {
  return rootGet<GatewayHealth>('/health');
}

export function getGatewayVersion(): Promise<GatewayVersion> {
  return apiGet<GatewayVersion>('/version');
}
