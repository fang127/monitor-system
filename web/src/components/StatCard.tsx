type StatCardProps = {
  label: string;
  value: string | number;
  tone?: "neutral" | "success" | "warning" | "danger";
  helper?: string;
};

export function StatCard({
  label,
  value,
  tone = "neutral",
  helper,
}: StatCardProps) {
  return (
    <article className={`stat-card stat-card-${tone}`}>
      <span className="stat-label">{label}</span>
      <strong className="stat-value">{value}</strong>
      {helper ? <span className="stat-helper">{helper}</span> : null}
    </article>
  );
}
