export type SortOrder = 'asc' | 'desc';
export type ServerStatus = 'ONLINE' | 'OFFLINE' | string;

export type ApiEnvelope<T> = {
  code: number;
  message: string;
  data?: T;
};

export type AsyncState<T> = {
  data: T | null;
  loading: boolean;
  error: string | null;
};

export type PaginationParams = {
  page?: number;
  page_size?: number;
};

export type TimeRangeParams = {
  start_time?: string;
  end_time?: string;
};

export type PagedQueryParams = PaginationParams & TimeRangeParams;

export type ServerScoreSummary = {
  server_name: string;
  score: number;
  last_update: string;
  status: ServerStatus | number;
  cpu_percent: number;
  mem_used_percent: number;
  disk_util_percent: number;
  load_avg_1: number;
};

export type ClusterStats = {
  total_servers: number;
  online_servers: number;
  offline_servers: number;
  avg_score: number;
  max_score: number;
  min_score: number;
  best_server: string;
  worst_server: string;
};

export type QueryLatestScoreResponse = {
  servers: ServerScoreSummary[];
  cluster_stats: ClusterStats;
};

export type QueryScoreRankResponse = {
  servers: ServerScoreSummary[];
  total_count: number;
  page: number;
  page_size: number;
};

export type PerformanceRecord = {
  server_name: string;
  timestamp: string;
  cpu_percent: number;
  usr_percent: number;
  system_percent: number;
  nice_percent: number;
  idle_percent: number;
  io_wait_percent: number;
  irq_percent: number;
  soft_irq_percent: number;
  load_avg_1: number;
  load_avg_3: number;
  load_avg_15: number;
  mem_used_percent: number;
  mem_total: number;
  mem_free: number;
  mem_avail: number;
  disk_util_percent: number;
  send_rate: number;
  rcv_rate: number;
  score: number;
  cpu_percent_rate: number;
  usr_percent_rate: number;
  system_percent_rate: number;
  io_wait_percent_rate: number;
  load_avg_1_rate: number;
  load_avg_3_rate: number;
  load_avg_15_rate: number;
  mem_used_percent_rate: number;
  disk_util_percent_rate: number;
  send_rate_rate: number;
  rcv_rate_rate: number;
};

export type QueryPerformanceResponse = {
  records: PerformanceRecord[];
  total_count: number;
  page: number;
  page_size: number;
};

export type QueryTrendResponse = {
  records: PerformanceRecord[];
  interval_seconds: number;
};

export type AnomalyRecord = {
  server_name: string;
  timestamp: string;
  anomaly_type: string;
  severity: string;
  value: number;
  threshold: number;
  metric_name: string;
};

export type QueryAnomalyResponse = {
  anomalies: AnomalyRecord[];
  total_count: number;
  page: number;
  page_size: number;
};

export type NetDetailRecord = {
  server_name: string;
  net_name: string;
  timestamp: string;
  err_in: string | number;
  err_out: string | number;
  drop_in: string | number;
  drop_out: string | number;
  rcv_bytes_rate: number;
  snd_bytes_rate: number;
  rcv_packets_rate: number;
  snd_packets_rate: number;
  rcv_bytes_rate_rate: number;
  snd_bytes_rate_rate: number;
  err_in_rate: number;
  err_out_rate: number;
  drop_in_rate: number;
  drop_out_rate: number;
};

export type DiskDetailRecord = {
  server_name: string;
  disk_name: string;
  timestamp: string;
  read_bytes_per_sec: number;
  write_bytes_per_sec: number;
  read_iops: number;
  write_iops: number;
  avg_read_latency_ms: number;
  avg_write_latency_ms: number;
  util_percent: number;
  read_bytes_per_sec_rate: number;
  write_bytes_per_sec_rate: number;
  read_iops_rate: number;
  write_iops_rate: number;
  util_percent_rate: number;
};

export type MemDetailRecord = {
  server_name: string;
  timestamp: string;
  total: number;
  free: number;
  avail: number;
  buffers: number;
  cached: number;
  swap_cached: number;
  active: number;
  inactive: number;
  active_anon: number;
  inactive_anon: number;
  active_file: number;
  inactive_file: number;
  dirty: number;
  writeback: number;
  anon_pages: number;
  mapped: number;
  total_rate: number;
  free_rate: number;
  avail_rate: number;
  active_rate: number;
  inactive_rate: number;
};

export type SoftIrqDetailRecord = {
  server_name: string;
  cpu_name: string;
  timestamp: string;
  hi: string | number;
  timer: string | number;
  net_tx: string | number;
  net_rx: string | number;
  block: string | number;
  irq_poll: string | number;
  tasklet: string | number;
  sched: string | number;
  hrtimer: string | number;
  rcu: string | number;
  hi_rate: number;
  timer_rate: number;
  net_tx_rate: number;
  net_rx_rate: number;
  block_rate: number;
  sched_rate: number;
};

export type MysqlDetailRecord = {
  server_name: string;
  instance: string;
  timestamp: string;
  mysql_host: string;
  mysql_port: number;
  up: boolean;
  version: string;
  role: string;
  max_connections: string | number;
  threads_connected: string | number;
  threads_running: string | number;
  aborted_connects: string | number;
  questions: string | number;
  com_select: string | number;
  com_insert: string | number;
  com_update: string | number;
  com_delete: string | number;
  com_commit: string | number;
  com_rollback: string | number;
  slow_queries: string | number;
  innodb_buffer_pool_read_requests: string | number;
  innodb_buffer_pool_reads: string | number;
  innodb_buffer_pool_hit_percent: number;
  innodb_row_lock_waits: string | number;
  innodb_row_lock_time_avg_ms: number;
  replication_configured: boolean;
  replication_running: boolean;
  replication_lag_seconds: number;
  connection_used_percent: number;
  qps: number;
  tps: number;
  slow_queries_rate: number;
  innodb_row_lock_waits_rate: number;
};

export type RedisDetailRecord = {
  server_name: string;
  instance: string;
  timestamp: string;
  redis_host: string;
  redis_port: number;
  up: boolean;
  version: string;
  role: string;
  uptime_in_seconds: string | number;
  connected_clients: string | number;
  blocked_clients: string | number;
  maxclients: string | number;
  connection_used_percent: number;
  used_memory: string | number;
  maxmemory: string | number;
  mem_fragmentation_ratio: number;
  memory_used_percent: number;
  total_commands_processed: string | number;
  instantaneous_ops_per_sec: number;
  commands_per_sec: number;
  keyspace_hits: string | number;
  keyspace_misses: string | number;
  keyspace_hit_percent: number;
  expired_keys: string | number;
  evicted_keys: string | number;
  rejected_connections: string | number;
  total_error_replies: string | number;
  total_net_input_bytes: string | number;
  total_net_output_bytes: string | number;
  net_input_bytes_per_sec: number;
  net_output_bytes_per_sec: number;
  replication_configured: boolean;
  master_link_up: boolean;
  connected_slaves: string | number;
  master_last_io_seconds_ago: number;
  slowlog_len: string | number;
  slowlog_growth: number;
};

export type QueryNetDetailResponse = {
  records: NetDetailRecord[];
  total_count: number;
  page: number;
  page_size: number;
};

export type QueryDiskDetailResponse = {
  records: DiskDetailRecord[];
  total_count: number;
  page: number;
  page_size: number;
};

export type QueryMemDetailResponse = {
  records: MemDetailRecord[];
  total_count: number;
  page: number;
  page_size: number;
};

export type QuerySoftIrqDetailResponse = {
  records: SoftIrqDetailRecord[];
  total_count: number;
  page: number;
  page_size: number;
};

export type QueryMysqlDetailResponse = {
  records: MysqlDetailRecord[];
  total_count: number;
  page: number;
  page_size: number;
};

export type QueryRedisDetailResponse = {
  records: RedisDetailRecord[];
  total_count: number;
  page: number;
  page_size: number;
};

export type GatewayHealth = {
  service: string;
  status: string;
};

export type GatewayVersion = {
  service: string;
  version: string;
};

export type DetailKind = 'net' | 'disk' | 'mem' | 'softirq' | 'mysql' | 'redis';

export type DetailResponseMap = {
  net: QueryNetDetailResponse;
  disk: QueryDiskDetailResponse;
  mem: QueryMemDetailResponse;
  softirq: QuerySoftIrqDetailResponse;
  mysql: QueryMysqlDetailResponse;
  redis: QueryRedisDetailResponse;
};

export type DetailRecordMap = {
  net: NetDetailRecord;
  disk: DiskDetailRecord;
  mem: MemDetailRecord;
  softirq: SoftIrqDetailRecord;
  mysql: MysqlDetailRecord;
  redis: RedisDetailRecord;
};
