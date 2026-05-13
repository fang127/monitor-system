import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom';
import { PageLayout } from './components/PageLayout';
import { AIOpsPage } from './pages/AIOpsPage';
import { AnomaliesPage } from './pages/AnomaliesPage';
import { Dashboard } from './pages/Dashboard';
import { DetailPage } from './pages/DetailPage';
import { PerformancePage } from './pages/PerformancePage';
import { ServerDetailPage } from './pages/ServerDetailPage';
import { ServersPage } from './pages/ServersPage';
import { SystemPage } from './pages/SystemPage';
import { TrendPage } from './pages/TrendPage';

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route element={<PageLayout />}>
          <Route index element={<Dashboard />} />
          <Route path="servers" element={<ServersPage />} />
          <Route path="servers/:server" element={<ServerDetailPage />} />
          <Route path="servers/:server/performance" element={<PerformancePage />} />
          <Route path="servers/:server/trend" element={<TrendPage />} />
          <Route path="servers/:server/anomalies" element={<AnomaliesPage />} />
          <Route path="servers/:server/details/:kind" element={<DetailPage />} />
          <Route path="ai-ops" element={<AIOpsPage />} />
          <Route path="system" element={<SystemPage />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}
