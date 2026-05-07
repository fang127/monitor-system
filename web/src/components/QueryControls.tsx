import type { ReactNode } from 'react';
import { useState } from 'react';
import { datetimeLocalToRfc3339, toDatetimeLocalValue } from '../utils/format';
import type { TimeRangeParams } from '../types/api';

type QueryControlsProps = {
  onApply: (params: TimeRangeParams) => void;
  extra?: ReactNode;
};

export function QueryControls({ onApply, extra }: QueryControlsProps) {
  const now = new Date();
  const [start, setStart] = useState(() => toDatetimeLocalValue(new Date(now.getTime() - 60 * 60 * 1000)));
  const [end, setEnd] = useState(() => toDatetimeLocalValue(now));

  return (
    <div className="query-controls">
      <label>
        开始时间
        <input type="datetime-local" value={start} onChange={(event) => setStart(event.target.value)} />
      </label>
      <label>
        结束时间
        <input type="datetime-local" value={end} onChange={(event) => setEnd(event.target.value)} />
      </label>
      {extra}
      <button
        type="button"
        onClick={() =>
          onApply({
            start_time: datetimeLocalToRfc3339(start),
            end_time: datetimeLocalToRfc3339(end),
          })
        }
      >
        查询
      </button>
    </div>
  );
}
