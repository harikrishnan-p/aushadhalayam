import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";

// @tabler/icons-react v3.44.0 has a packaging bug: the `module` field in its
// package.json points to dist/esm/tabler-icons-react.mjs which is not included
// in the published tarball. The CJS bundle does exist, so we alias Vite to it.
// Remove this alias if the package is upgraded to a version that ships the ESM file.
const tablerAlias = path.resolve(
  __dirname,
  "node_modules/@tabler/icons-react/dist/cjs/tabler-icons-react.cjs"
);

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      "@tabler/icons-react": tablerAlias,
    },
  },
  server: {
    port: 1420,
    strictPort: true,
  },
  build: {
    outDir: "dist",
    emptyOutDir: true,
  },
});
