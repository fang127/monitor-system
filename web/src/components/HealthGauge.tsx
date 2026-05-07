import { useEffect, useRef } from 'react';
import * as echarts from 'echarts/core';
import { GaugeChart } from 'echarts/charts';
import { TooltipComponent } from 'echarts/components';
import { CanvasRenderer } from 'echarts/renderers';
import type { EChartsOption } from 'echarts';
import type { EChartsType } from 'echarts/core';
import { EmptyState } from './SectionState';

echarts.use([GaugeChart, TooltipComponent, CanvasRenderer]);

type HealthGaugeProps = {
  score: number | null;
  label?: string;
};

export function HealthGauge({ score, label = '健康分' }: HealthGaugeProps) {
  const chartRef = useRef<HTMLDivElement | null>(null);
  const instanceRef = useRef<EChartsType | null>(null);

  useEffect(() => {
    if (!chartRef.current || typeof score !== 'number') {
      return undefined;
    }

    if (!instanceRef.current) {
      instanceRef.current = echarts.init(chartRef.current);
    }

    const value = Math.max(0, Math.min(100, score));
    const progressColor = value >= 80 ? '#14936f' : value >= 60 ? '#b7791f' : '#c2413b';
    const option: EChartsOption = {
      tooltip: { formatter: '{b}: {c}' },
      series: [
        {
          type: 'gauge',
          min: 0,
          max: 100,
          radius: '92%',
          progress: {
            show: true,
            width: 14,
            itemStyle: { color: progressColor },
          },
          axisLine: {
            lineStyle: {
              width: 14,
              color: [[1, '#e2e8f0']],
            },
          },
          axisTick: { show: false },
          splitLine: {
            distance: -18,
            length: 8,
            lineStyle: { color: '#94a3b8', width: 1 },
          },
          axisLabel: { color: '#64748b', distance: 18, fontSize: 11 },
          pointer: { width: 5, itemStyle: { color: '#334155' } },
          anchor: { show: true, size: 8, itemStyle: { color: '#334155' } },
          detail: {
            valueAnimation: true,
            formatter: '{value}',
            color: '#0f172a',
            fontSize: 36,
            fontWeight: 700,
            offsetCenter: [0, '62%'],
          },
          data: [{ value: Math.round(value), name: label }],
        },
      ],
    };

    instanceRef.current.setOption(option, true);
    const resizeObserver = new ResizeObserver(() => instanceRef.current?.resize());
    resizeObserver.observe(chartRef.current);

    return () => resizeObserver.disconnect();
  }, [label, score]);

  useEffect(() => {
    return () => {
      instanceRef.current?.dispose();
      instanceRef.current = null;
    };
  }, []);

  if (typeof score !== 'number') {
    return <EmptyState title="健康分" message="暂无健康分数据" />;
  }

  return <div ref={chartRef} className="health-chart" aria-label={`${label} ${score}`} />;
}
