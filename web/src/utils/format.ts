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

export type BytesRateUnit = 'B/s' | 'KB/s' | 'MB/s' | 'GB/s';

const bytesRateUnits: BytesRateUnit[] = ['B/s', 'KB/s', 'MB/s', 'GB/s'];

function bytesRateUnitIndex(unit: BytesRateUnit): number {
  return bytesRateUnits.indexOf(unit);
}

export function pickBytesRateUnit(values: Array<number | null | undefined>): BytesRateUnit {
  const maxValue = values.reduce<number>((max, value) => {
    if (typeof value !== 'number' || Number.isNaN(value)) {
      return max;
    }

    return Math.max(max, Math.abs(value));
  }, 0);

  let current = maxValue;
  let index = 0;

  while (current >= 1024 && index < bytesRateUnits.length - 1) {
    current /= 1024;
    index += 1;
  }

  return bytesRateUnits[index];
}

export function formatBytesRate(value: number | null | undefined, unit?: BytesRateUnit): string {
  if (typeof value !== 'number' || Number.isNaN(value)) {
    return '--';
  }

  let current = Math.abs(value);
  let index = unit ? bytesRateUnitIndex(unit) : 0;

  if (unit) {
    current /= 1024 ** index;
  }

  while (!unit && current >= 1024 && index < bytesRateUnits.length - 1) {
    current /= 1024;
    index += 1;
  }

  const signed = value < 0 ? -current : current;
  return `${signed.toFixed(1)} ${bytesRateUnits[index]}`;
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
