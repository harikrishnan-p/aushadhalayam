# Aushadhalayam — Pharmacy POS & GST Compliance System

A dual-client monorepo for Indian retail pharmacy management. Handles billing, inventory (FEFO batch dispatch), and GST reporting (GSTR-1/GSTR-2B) across a wide range of pharmacy hardware — from Windows XP SP3 machines to modern Android tablets — using a shared SQLite schema with automatic cloud synchronisation via a Last-Writer-Wins replication queue.

| Client | Target | Stack |
|--------|--------|-------|
| `client-legacy` | Windows XP SP3 – Windows 11 (32-bit) | C++11 · wxWidgets 3.2 · MinGW-w64 |
| `client-modern` | Windows 10/11 · Android | Tauri 2 · Rust · Tokio · rusqlite |

Both clients share `docs/schema.sql` as their ground truth and replicate mutations to a central PostgreSQL instance through an append-only `sync_outbox` table populated by SQLite triggers.

See [BUILDING.md](BUILDING.md) for full build and test instructions.
