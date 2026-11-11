import type { LucideIcon } from "lucide-react";

export function Metric({
  icon: Icon,
  label,
  value,
  hint,
  color = "blue",
}: {
  icon: LucideIcon;
  label: string;
  value: string;
  hint: string;
  color?: string;
}) {
  return (
    <article className="metric card">
      <div className="metric-top">
        <span>{label}</span>
        <span className={`metric-icon ${color}`}>
          <Icon size={18} />
        </span>
      </div>
      <strong className="metric-value">{value}</strong>
      <p>{hint}</p>
    </article>
  );
}
