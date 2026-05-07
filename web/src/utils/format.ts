export function formatPercent(value: number | null | undefined): string {
  if (typeof value !== 'number' || Number.isNaN(value)) {
    return '--';
  }

  return `${value.toFixed(1)}%`;
}

export function formatNumber(value: number | string | null | undefined, fractionDigits = 1): string {
  if (typeof value === 'string') {
    return value || '--';
  }

  if (typeof value !== 'number' || Number.isNaN(value)) {
    return '--';
  }

  return value.toLocaleString(undefined, {
    maximumFractionDigits: fractionDigits,
    minimumFractionDigits: fractionDigits,
  });
}

export function formatScore(value: number | null | undefined): string {
  if (typeof value !== 'number' || Number.isNaN(value)) {
    return '--';
  }

  return Math.max(0, Math.min(100, value)).toFixed(0);
}

export function formatBytesRate(value: number | null | undefined): string {
  if (typeof value !== 'number' || Number.isNaN(value)) {
    return '--';
  }

  const units = ['B/s', 'KB/s', 'MB/s', 'GB/s'];
  let current = Math.abs(value);
  let index = 0;

  while (current >= 1024 && index < units.length - 1) {
    current /= 1024;
    index += 1;
  }

  const signed = value < 0 ? -current : current;
  return `${signed.toFixed(1)} ${units[index]}`;
}

export function formatDateTime(value: string | null | undefined): string {
  if (!value) {
    return '--';
  }

  const date = new Date(value);

  if (Number.isNaN(date.getTime())) {
    return '--';
  }

  return date.toLocaleString();
}

export function toDatetimeLocalValue(date: Date): string {
  const offsetDate = new Date(date.getTime() - date.getTimezoneOffset() * 60000);
  return offsetDate.toISOString().slice(0, 16);
}

export function datetimeLocalToRfc3339(value: string): string | undefined {
  if (!value) {
    return undefined;
  }

  const date = new Date(value);
  if (Number.isNaN(date.getTime())) {
    return undefined;
  }

  return date.toISOString();
}

export function scoreTone(score: number): 'success' | 'warning' | 'danger' {
  if (score >= 80) {
    return 'success';
  }
  if (score >= 60) {
    return 'warning';
  }
  return 'danger';
}
