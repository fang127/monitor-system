import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '');
  const backendTarget = env.VITE_API_PROXY_TARGET || 'http://localhost:8080';

  return {
    plugins: [react()],
    build: {
      rollupOptions: {
        output: {
          manualChunks: {
            charts: ['echarts', 'zrender'],
            react: ['react', 'react-dom', 'react-router-dom'],
          },
        },
      },
    },
    server: {
      proxy: {
        '/api': {
          target: backendTarget,
          changeOrigin: true,
        },
        '/health': {
          target: backendTarget,
          changeOrigin: true,
        },
      },
    },
  };
});
