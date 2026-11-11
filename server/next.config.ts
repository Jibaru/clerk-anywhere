import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  output: "standalone",
  turbopack: {
    rules: {
      "*.css": {
        loaders: ["@tailwindcss/turbopack"],
        as: "*.css",
      },
    },
  },
  // Consoles print their URL as an uppercase-only QR code (alphanumeric mode
  // keeps the code small), so the uppercase path must resolve too.
  async rewrites() {
    return [{ source: "/G/:challenge", destination: "/g/:challenge" }];
  },
};

export default nextConfig;
