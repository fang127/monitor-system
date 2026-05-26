import { useEffect, useRef } from "react";
import * as echarts from "echarts/core";
import { LineChart } from "echarts/charts";
import {
  GridComponent,
  LegendComponent,
  TooltipComponent,
} from "echarts/components";
import { CanvasRenderer } from "echarts/renderers";
import type { EChartsOption } from "echarts";
import type { EChartsType } from "echarts/core";
import { EmptyState } from "./SectionState";

echarts.use([
  LineChart,
  GridComponent,
  LegendComponent,
  TooltipComponent,
  CanvasRenderer,
]);

export type ChartSeries<T extends object> = {
  key: keyof T;
  name: string;
};

type MetricLineChartProps<T extends { timestamp: string }> = {
  rows: T[];
  series: ChartSeries<T>[];
  title: string;
};

export function MetricLineChart<T extends { timestamp: string }>({
  rows,
  series,
  title,
}: MetricLineChartProps<T>) {
  const chartRef = useRef<HTMLDivElement | null>(null);
  const instanceRef = useRef<EChartsType | null>(null);

  useEffect(() => {
    if (!chartRef.current || rows.length === 0) {
      return undefined;
    }

    if (!instanceRef.current) {
      instanceRef.current = echarts.init(chartRef.current);
    }

    const option: EChartsOption = {
      tooltip: { trigger: "axis" },
      legend: { top: 0, textStyle: { color: "#475569" } },
      grid: { left: 48, right: 18, top: 44, bottom: 36 },
      xAxis: {
        type: "category",
        boundaryGap: false,
        data: rows.map((row) => new Date(row.timestamp).toLocaleTimeString()),
        axisLabel: { color: "#64748b" },
      },
      yAxis: {
        type: "value",
        axisLabel: { color: "#64748b" },
        splitLine: { lineStyle: { color: "#e5ecf3" } },
      },
      series: series.map((item) => ({
        type: "line",
        name: item.name,
        smooth: true,
        showSymbol: false,
        data: rows.map((row) => Number(row[item.key] ?? 0)),
      })),
    };

    instanceRef.current.setOption(option, true);
    const resizeObserver = new ResizeObserver(() =>
      instanceRef.current?.resize(),
    );
    resizeObserver.observe(chartRef.current);

    return () => resizeObserver.disconnect();
  }, [rows, series]);

  useEffect(() => {
    return () => {
      instanceRef.current?.dispose();
      instanceRef.current = null;
    };
  }, []);

  if (rows.length === 0) {
    return <EmptyState title={title} message="暂无可绘制数据" />;
  }

  return <div className="metric-chart" ref={chartRef} aria-label={title} />;
}
