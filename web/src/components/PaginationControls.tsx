type PaginationControlsProps = {
  page: number;
  pageSize: number;
  totalCount?: number;
  onPageChange: (page: number) => void;
  onPageSizeChange: (pageSize: number) => void;
};

export function PaginationControls({
  page,
  pageSize,
  totalCount,
  onPageChange,
  onPageSizeChange,
}: PaginationControlsProps) {
  const totalPages = totalCount
    ? Math.max(1, Math.ceil(totalCount / pageSize))
    : undefined;
  const canGoNext = totalPages ? page < totalPages : true;

  return (
    <div className="pagination-row">
      <span>
        {typeof totalCount === "number" ? `共 ${totalCount} 条` : "分页查询"}
      </span>
      <label>
        每页
        <select
          value={pageSize}
          onChange={(event) => onPageSizeChange(Number(event.target.value))}
        >
          <option value={20}>20</option>
          <option value={50}>50</option>
          <option value={100}>100</option>
        </select>
      </label>
      <button
        type="button"
        onClick={() => onPageChange(Math.max(1, page - 1))}
        disabled={page <= 1}
      >
        上一页
      </button>
      <strong>{totalPages ? `${page} / ${totalPages}` : page}</strong>
      <button
        type="button"
        onClick={() => onPageChange(page + 1)}
        disabled={!canGoNext}
      >
        下一页
      </button>
    </div>
  );
}
