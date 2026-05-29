import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import { AuthProvider } from "./auth/AuthContext";
import { PageLayout } from "./components/PageLayout";
import { AdminRoute, ProtectedRoute } from "./components/ProtectedRoute";
import { AIOpsPage } from "./pages/AIOpsPage";
import { AnomaliesPage } from "./pages/AnomaliesPage";
import { Dashboard } from "./pages/Dashboard";
import { DetailPage } from "./pages/DetailPage";
import { LoginPage } from "./pages/LoginPage";
import { PerformancePage } from "./pages/PerformancePage";
import { ServerDetailPage } from "./pages/ServerDetailPage";
import { ServersPage } from "./pages/ServersPage";
import { SystemPage } from "./pages/SystemPage";
import { TrendPage } from "./pages/TrendPage";
import { UsersPage } from "./pages/UsersPage";

export default function App() {
  return (
    <AuthProvider>
      <BrowserRouter>
        <Routes>
          <Route path="login" element={<LoginPage />} />
          <Route element={<ProtectedRoute />}>
            <Route element={<PageLayout />}>
              <Route index element={<Dashboard />} />
              <Route path="servers" element={<ServersPage />} />
              <Route path="servers/:server" element={<ServerDetailPage />} />
              <Route
                path="servers/:server/performance"
                element={<PerformancePage />}
              />
              <Route path="servers/:server/trend" element={<TrendPage />} />
              <Route
                path="servers/:server/anomalies"
                element={<AnomaliesPage />}
              />
              <Route
                path="servers/:server/details/:kind"
                element={<DetailPage />}
              />
              <Route path="ai-ops" element={<AIOpsPage />} />
              <Route path="system" element={<SystemPage />} />
              <Route element={<AdminRoute />}>
                <Route path="users" element={<UsersPage />} />
              </Route>
              <Route path="*" element={<Navigate to="/" replace />} />
            </Route>
          </Route>
        </Routes>
      </BrowserRouter>
    </AuthProvider>
  );
}
