import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// /api is proxied to lego_server so no CORS setup is needed in dev.
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      "/api": "http://127.0.0.1:8080",
    },
  },
});
