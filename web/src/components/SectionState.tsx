type SectionStateProps = {
  title: string;
  message?: string;
};

export function LoadingState({ title, message = '正在加载数据' }: SectionStateProps) {
  return (
    <div className="section-state" role="status" aria-live="polite">
      <span className="spinner" aria-hidden="true" />
      <strong>{title}</strong>
      <span>{message}</span>
    </div>
  );
}

export function ErrorState({ title, message = '接口暂不可用' }: SectionStateProps) {
  return (
    <div className="section-state section-state-error" role="alert">
      <strong>{title}</strong>
      <span>{message}</span>
    </div>
  );
}

export function EmptyState({ title, message = '暂无数据' }: SectionStateProps) {
  return (
    <div className="section-state">
      <strong>{title}</strong>
      <span>{message}</span>
    </div>
  );
}
