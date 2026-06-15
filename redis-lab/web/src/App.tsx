import { useEffect } from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import { useAuth } from './stores/authStore';
import LoginPage from './pages/LoginPage';
import WorkspacePage from './pages/WorkspacePage';

function RequireAuth({ children }: { children: React.ReactNode }) {
  const token = useAuth((s) => s.token);
  if (!token) return <Navigate to="/login" replace />;
  return <>{children}</>;
}

export default function App() {
  const hydrate = useAuth((s) => s.hydrate);
  useEffect(() => { hydrate(); }, []);

  return (
    <Routes>
      <Route path="/login" element={<LoginPage />} />
      <Route path="/*" element={<RequireAuth><WorkspacePage /></RequireAuth>} />
    </Routes>
  );
}
