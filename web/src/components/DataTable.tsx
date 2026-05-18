import type { ReactNode } from 'react';
import { EmptyState } from './SectionState';

type RowValue = string | number | boolean | null | undefined;

export type Column<T extends object> = {
  key: keyof T | string;
  title: string;
  className?: string;
  render?: (row: T) => ReactNode;
};

type DataTableProps<T extends object> = {
  rows: T[];
  columns: Column<T>[];
  emptyTitle: string;
  emptyMessage?: string;
};

function getValue<T extends object>(row: T, key: keyof T | string): RowValue {
  return (row as Record<string, RowValue>)[String(key)];
}

export function DataTable<T extends object>({ rows, columns, emptyTitle, emptyMessage }: DataTableProps<T>) {
  if (rows.length === 0) {
    return <EmptyState title={emptyTitle} message={emptyMessage || '暂无数据'} />;
  }

  return (
    <div className="table-wrap">
      <table className="data-table">
        <thead>
          <tr>
            {columns.map((column) => (
              <th key={String(column.key)} className={column.className}>
                {column.title}
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {rows.map((row, index) => (
            <tr key={index}>
              {columns.map((column) => (
                <td key={String(column.key)} className={column.className}>
                  {column.render ? column.render(row) : getValue(row, column.key) ?? '--'}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
