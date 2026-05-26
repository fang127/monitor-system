import { useEffect, useState } from "react";
import { getScoreRank } from "../api/dashboard";
import { PaginationControls } from "../components/PaginationControls";
import { ErrorState, LoadingState } from "../components/SectionState";
import { ServerTable } from "../components/ServerTable";
import type {
  AsyncState,
  QueryScoreRankResponse,
  SortOrder,
} from "../types/api";

function initialState(): AsyncState<QueryScoreRankResponse> {
  return { data: null, loading: true, error: null };
}

export function ServersPage() {
  const [order, setOrder] = useState<SortOrder>("desc");
  const [page, setPage] = useState(1);
  const [pageSize, setPageSize] = useState(20);
  const [rank, setRank] =
    useState<AsyncState<QueryScoreRankResponse>>(initialState);

  useEffect(() => {
    let active = true;
    setRank((current) => ({ ...current, loading: true, error: null }));
    getScoreRank({ order, page, page_size: pageSize })
      .then((data) => {
        if (active) {
          setRank({ data, loading: false, error: null });
        }
      })
      .catch((error: Error) => {
        if (active) {
          setRank({ data: null, loading: false, error: error.message });
        }
      });

    return () => {
      active = false;
    };
  }, [order, page, pageSize]);

  return (
    <div className="page-stack">
      <header className="page-header">
        <div>
          <span className="eyebrow">GET /api/servers/score-rank</span>
          <h1>服务器评分列表</h1>
        </div>
        <label className="inline-control">
          排序
          <select
            value={order}
            onChange={(event) => {
              setPage(1);
              setOrder(event.target.value as SortOrder);
            }}
          >
            <option value="desc">高分优先</option>
            <option value="asc">低分优先</option>
          </select>
        </label>
      </header>

      <section className="section-block">
        {rank.loading ? <LoadingState title="评分列表" /> : null}
        {rank.error ? (
          <ErrorState title="评分列表" message={rank.error} />
        ) : null}
        {!rank.loading && !rank.error ? (
          <ServerTable data={rank.data?.servers || []} />
        ) : null}
        {!rank.loading && !rank.error ? (
          <PaginationControls
            page={rank.data?.page || page}
            pageSize={rank.data?.page_size || pageSize}
            totalCount={rank.data?.total_count}
            onPageChange={setPage}
            onPageSizeChange={(nextPageSize) => {
              setPage(1);
              setPageSize(nextPageSize);
            }}
          />
        ) : null}
      </section>
    </div>
  );
}
