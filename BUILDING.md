# Aushadhalayam — Build & Test Guide

## Repository layout

```
aushadhalayam/
├── docs/
│   └── schema.sql              # Ground-truth SQLite schema (shared by both clients)
├── client-legacy/              # C++11 · wxWidgets 3.2 · MinGW-w64
│   ├── CMakeLists.txt
│   ├── vendor/sqlite3/         # SQLite amalgamation — YOU must download this
│   └── src/
│       ├── main.cpp            # wxApp entry point
│       ├── MainFrame.h/.cpp    # Primary POS window
│       ├── WorkerThread.h/.cpp # Background SQLite thread (wxThread)
│       ├── BillingGrid.h/.cpp  # Virtual wxGridTableBase billing ledger
│       ├── DatabaseManager.h/.cpp
│       ├── GstEngine.h         # Header-only GST computation
│       └── PrinterManager.h/.cpp  # GDI Spooler + ESC/POS thermal printer
└── client-modern/              # Tauri 2 · Rust · Tokio
    └── src-tauri/
        ├── Cargo.toml
        ├── build.rs
        ├── schema.sql          # Copy of docs/schema.sql (embedded via include_str!)
        ├── tauri.conf.json
        └── src/
            ├── main.rs
            ├── state.rs
            ├── db/mod.rs       # Connection open + WAL config + schema bootstrap
            ├── db/sync.rs      # Background outbox → cloud sync agent
            ├── commands/       # Tauri IPC command handlers
            │   ├── billing.rs  # process_checkout, cancel_bill
            │   ├── inventory.rs
            │   └── gst.rs
            └── printer/mod.rs  # cfg-gated: GDI (Windows) / TCP ESC/POS (Android)
```

---

## Prerequisites

### Common
- **Git** (any modern version)
- **SQLite amalgamation** — download `sqlite-amalgamation-*.zip` from  
  <https://sqlite.org/download.html> and unpack `sqlite3.c` + `sqlite3.h`  
  into `client-legacy/vendor/sqlite3/`

### Legacy client
| Tool | Version | Notes |
|------|---------|-------|
| MinGW-w64 (GCC) | 8.x – 13.x | i686 for 32-bit XP-compatible output; x86_64 also works on Win7+ |
| CMake | ≥ 3.10 | Must be in PATH |
| wxWidgets | 3.2.x | Must be **static** build (see below) |

### Modern client
| Tool | Version | Notes |
|------|---------|-------|
| Rust + Cargo | stable (≥ 1.77) | `rustup update stable` |
| Tauri CLI | 2.x | `cargo install tauri-cli --version "^2"` |
| Node.js | ≥ 18 LTS | For the web frontend |
| npm / pnpm | any | Package manager for the frontend |
| Android SDK + NDK | NDK r25c+ | Only for Android target |

---

## Building `client-legacy` (C++ / wxWidgets)

### Step 1 — Build wxWidgets statically with MinGW

Download wxWidgets 3.2.x source from <https://www.wxwidgets.org/downloads/>.

```bash
# From the wxWidgets source root
cd build/msw
mingw32-make -f makefile.gcc \
    BUILD=release \
    SHARED=0 \
    UNICODE=1 \
    MONOLITHIC=0 \
    -j4
```

This produces static `.a` libraries in `lib/gcc_lib/`. Note the full path —  
you will pass it as `wxWidgets_ROOT_DIR` below.

### Step 2 — Populate the SQLite amalgamation

```
client-legacy/vendor/sqlite3/sqlite3.c   ← from sqlite-amalgamation-*.zip
client-legacy/vendor/sqlite3/sqlite3.h   ← from sqlite-amalgamation-*.zip
```

### Step 3 — Configure and build

```bash
cd client-legacy
mkdir build && cd build

cmake .. \
  -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DwxWidgets_ROOT_DIR="C:/path/to/wxWidgets-3.2.x"

mingw32-make -j4
```

Output: `build/bin/aushadhalayam.exe`

The executable is **fully static** — no MinGW or MSVC runtime DLLs. Verify with:

```bash
objdump -p build/bin/aushadhalayam.exe | grep "DLL Name"
# Expected: only Windows system DLLs (kernel32, user32, gdi32, comctl32, etc.)
# Must NOT list: libgcc_s_*.dll, libstdc++-6.dll, libwinpthread-1.dll
```

### Step 4 — First run

Copy `docs/schema.sql` next to the executable (or embed it in a resource file  
and pass it to `DatabaseManager::apply_schema()`). On launch, the app:

1. Opens / creates `pharmacy.db` in the same directory as the `.exe`
2. Applies the schema (all `CREATE TABLE IF NOT EXISTS` — safe to re-run)
3. Registers the device in the `devices` table
4. Starts the `PharmacyWorkerThread`

### Compiler flags reference

| Flag | Purpose |
|------|---------|
| `-static -static-libgcc -static-libstdc++` | Bundle all runtime into the EXE |
| `-Wl,--subsystem,windows:5.01` | Set minimum OS to Windows XP SP3 (PE header) |
| `-mwindows` | Windowed subsystem; suppress console |
| `-O2 -ffunction-sections -fdata-sections -Wl,--gc-sections` | Size/speed optimisation + dead-code strip |

### Debug build (with console)

```bash
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
mingw32-make -j4
# Debug build omits -mwindows so a console window shows log output
```

---

## Building `client-modern` (Tauri 2 / Rust)

### Step 1 — Generate application icons

Tauri requires icon files in `client-modern/src-tauri/icons/`.  
Generate all required sizes from a single 1024×1024 source PNG:

```bash
cd client-modern
npm run tauri icon -- path/to/your-app-icon-1024.png
```

This creates `icons/32x32.png`, `icons/128x128.png`, `icons/icon.ico`, `icons/icon.icns`, etc.  
You only need to do this once, or whenever you update the icon.

### Step 2 — Keep schema in sync

`client-modern/src-tauri/schema.sql` must be identical to `docs/schema.sql`.  
The Rust binary embeds it at compile time via `include_str!`.

```bash
# From the repo root — run this whenever docs/schema.sql changes
copy docs\schema.sql client-modern\src-tauri\schema.sql
```

### Step 3 — Install frontend dependencies

```bash
cd client-modern
npm install          # or: pnpm install
```

> The frontend framework is not prescribed. Use Svelte, React, Vue, or plain  
> HTML/JS. The Tauri `devUrl` in `tauri.conf.json` points to `http://localhost:1420`.

### Step 4 — Development mode (Windows)

```bash
cd client-modern
npm run tauri dev
```

This starts the Vite dev server and the Rust Tauri backend in a single hot-reload  
session. SQLite database opens at the directory of the compiled binary.

### Step 5 — Release build (Windows)

```bash
npm run tauri build
```

Output: `client-modern/src-tauri/target/release/bundle/`  
- `.msi` installer (Windows)  
- Standalone `.exe`

### Step 6 — Android build

```bash
# Add the Android target to Rust
rustup target add aarch64-linux-android

# Initialise Tauri Android project (first time only)
npm run tauri android init

# Debug build on connected device / emulator
npm run tauri android dev

# Release APK / AAB
npm run tauri android build
```

Set `ANDROID_HOME` and `NDK_HOME` environment variables before running.

### Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SYNC_ENDPOINT` | `https://sync.aushadhalayam.local/api/ingest` | Cloud sync URL |
| `RUST_LOG` | `info` | Log level (`trace`, `debug`, `info`, `warn`, `error`) |

---

## Testing

### Legacy client — manual smoke test checklist

Because the legacy client targets XP SP3 and has no test framework dependency,  
validation is done by running the application:

1. **Schema bootstrap** — delete `pharmacy.db`, launch the exe. No crash = schema applied.
2. **Product search** — type a partial product name in the search box; results should appear within 200 ms.
3. **Checkout flow**
   - Add ≥ 2 products from different batches
   - Edit quantity inline in the grid; totals update
   - Click **CHECKOUT**; verify the bill number appears and `pharmacy.db` has a new row in `bills`
4. **Stock deduction** — query `stock_batches` before and after checkout; `quantity` should decrease.
5. **Sync outbox** — after checkout, `sync_outbox` should have new rows with `is_transmitted=0`.
6. **Printer (ESC/POS)** — set `printer.cfg` to `mode=COM name=COM1 baud_rate=9600`; run checkout; receipt should print.
7. **XP compatibility** — copy the `.exe` to a Windows XP SP3 VM and repeat steps 1–4.

### Modern client — Rust unit tests

```bash
cd client-modern/src-tauri

# Run all Rust unit tests (no Tauri window needed)
cargo test

# Run with log output
RUST_LOG=debug cargo test -- --nocapture
```

Key things tested by the default test suite (add these as you flesh out the codebase):

| Module | What to test |
|--------|-------------|
| `db/mod.rs` | Schema applies idempotently; WAL PRAGMAs are set |
| `commands/billing.rs` | `compute_line()` GST math for each slab (0%, 5%, 12%, 18%) |
| `commands/billing.rs` | `process_checkout` rolls back when stock is insufficient |
| `db/sync.rs` | `fetch_pending_rows` respects `MAX_RETRIES` limit |
| `printer/mod.rs` | `build_escpos` output starts with `ESC @` (0x1B 0x40) |

Example unit test for GST rounding:

```rust
// in commands/billing.rs or a tests/ module
#[cfg(test)]
mod tests {
    use super::{compute_line, CartItem};

    fn make_item(mrp: f64, qty: i64, disc: f64, rate: f64) -> CartItem {
        CartItem {
            product_id: 1, batch_id: 1,
            product_name: "Test".into(), hsn_code: "3004".into(),
            batch_number: "B1".into(), expiry_date: "2027-12".into(),
            quantity: qty, mrp, discount_pct: disc, gst_rate: rate,
        }
    }

    #[test]
    fn gst_12_percent_intra_state() {
        let line = compute_line(&make_item(112.0, 1, 0.0, 12.0), false);
        // MRP 112 is GST-inclusive at 12%
        // taxable = 112 / 1.12 = 100.00
        // cgst = sgst = 6.00
        assert!((line.taxable_value - 100.0).abs() < 0.01);
        assert!((line.cgst_amount  -   6.0).abs() < 0.01);
        assert!((line.sgst_amount  -   6.0).abs() < 0.01);
        assert!((line.line_total   - 112.0).abs() < 0.01);
    }

    #[test]
    fn stock_deduction_is_fefo() {
        // Integration test: open in-memory DB, insert two batches,
        // checkout the earlier-expiry batch first.
        // Requires spinning up a rusqlite Connection — see tests/checkout_tests.rs
    }
}
```

### Integration test — SQLite trigger verification

Verify that the sync outbox is populated atomically:

```bash
# Using the sqlite3 CLI
sqlite3 pharmacy.db

-- Before checkout
SELECT COUNT(*) FROM sync_outbox WHERE is_transmitted=0;

-- Run a checkout via the UI or via a direct SQL INSERT (for testing)

-- After checkout: count should increase by (1 bill + N items + N batch updates)
SELECT table_name, operation, COUNT(*)
  FROM sync_outbox
 WHERE is_transmitted=0
 GROUP BY 1,2;
```

---

## Printer setup

### ESC/POS via serial (COM port) — legacy client

Create `printer.cfg` next to `aushadhalayam.exe`:

```ini
mode=COM
name=COM3
baud_rate=9600
paper_width_mm=80
cut_paper=true
```

### ESC/POS via serial — modern client

Set in your app config or environment:

```json
{
  "mode": "escpos_com",
  "port": "COM3",
  "baud_rate": 9600,
  "paper_width_mm": 80,
  "cut_paper": true
}
```

### ESC/POS over Wi-Fi network — modern client (Android)

```json
{
  "mode": "escpos_net",
  "host": "192.168.1.200",
  "port": 9100,
  "paper_width_mm": 80,
  "cut_paper": true
}
```

### Windows GDI (any installed printer driver) — modern client

```json
{
  "mode": "gdi",
  "printer_name": "Epson TM-T82",
  "paper_width_mm": 80
}
```

---

## Sync endpoint contract

The `sync_outbox` rows are POST-ed as a JSON array to `SYNC_ENDPOINT`:

```json
[
  {
    "id": 42,
    "table_name": "bills",
    "operation": "INSERT",
    "row_pk": "{\"id\":17}",
    "payload": "{ ...full row JSON... }",
    "created_at_us": 1750758412345678,
    "device_id": "abc12345-..."
  }
]
```

The server upserts using the LWW strategy documented in `docs/schema.sql`  
(see the comment block at the bottom of that file).

---

## Useful one-liners

```bash
# Check pending sync depth
sqlite3 pharmacy.db "SELECT COUNT(*) FROM sync_outbox WHERE is_transmitted=0;"

# List near-expiry stock (expires within 3 months)
sqlite3 pharmacy.db \
  "SELECT p.name, sb.batch_number, sb.expiry_date, sb.quantity
     FROM stock_batches sb JOIN products p ON p.id=sb.product_id
    WHERE sb.expiry_date <= strftime('%Y-%m','now','+3 months')
      AND sb.quantity > 0 ORDER BY sb.expiry_date;"

# Monthly GST summary
sqlite3 pharmacy.db \
  "SELECT month, year, hsn_code, gst_rate,
          ROUND(taxable_value,2), ROUND(total_cgst,2), ROUND(total_sgst,2)
     FROM v_gstr1_b2c WHERE year='2026' ORDER BY month, hsn_code;"

# Verify EXE has no unexpected DLL dependencies (legacy)
objdump -p client-legacy/build/bin/aushadhalayam.exe | grep "DLL Name"
```
