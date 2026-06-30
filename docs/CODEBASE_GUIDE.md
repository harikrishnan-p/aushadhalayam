# Aushadhalayam — Codebase Guide

A plain-language walkthrough of every file in the project, written so that someone who has never seen the code before can understand what each piece does, why it exists, and how everything connects.

---

## Table of Contents

1. [What this software does](#1-what-this-software-does)
2. [Repository layout](#2-repository-layout)
3. [The shared database — `docs/schema.sql`](#3-the-shared-database)
4. [The modern client — `client-modern/`](#4-the-modern-client)
   - [The Rust backend — `src-tauri/`](#41-the-rust-backend)
   - [The React frontend — `src/`](#42-the-react-frontend)
5. [The legacy client — `client-legacy/`](#5-the-legacy-client)
6. [How the two clients share data](#6-how-the-two-clients-share-data)
7. [Key concepts explained](#7-key-concepts-explained)

---

## 1. What this software does

Aushadhalayam is a **pharmacy point-of-sale (POS) system**. A pharmacist uses it to:

- Sell medicines and print a receipt for the customer
- Keep track of how much stock is in the shop (inventory)
- Manage which medicines are in the catalogue and their GST tax rates
- Record incoming stock from suppliers (purchase orders)
- Generate monthly tax reports required by Indian law (GSTR-1)
- Sync all data to a cloud server so multiple shop terminals stay in sync

The project has **two separate applications** that do the same job. The reason for two clients is that different shops use different operating systems and have different hardware requirements:

| Client | Technology | Target environment |
|---|---|---|
| `client-modern` | Tauri (Rust) + React (TypeScript) | Modern Windows 10/11 PCs, future Android tablets |
| `client-legacy` | C++11 + wxWidgets | Older Windows XP through Windows 11 machines |

Both clients talk to the **same SQLite database file** on disk and use the **same schema**.

---

## 2. Repository layout

```
aushadhalayam/
│
├── docs/
│   ├── schema.sql          ← The single source of truth for the database structure
│   └── CODEBASE_GUIDE.md   ← This file
│
├── client-modern/          ← Tauri + React application
│   ├── package.json        ← JavaScript package list
│   ├── vite.config.ts      ← Build tool configuration
│   ├── tsconfig.json       ← TypeScript compiler settings
│   ├── src/                ← React user interface code
│   └── src-tauri/          ← Rust backend code
│       ├── Cargo.toml      ← Rust package list
│       ├── build.rs        ← Build script
│       ├── tauri.conf.json ← App window and packaging settings
│       └── src/            ← Rust source files
│
├── client-legacy/          ← C++/wxWidgets application
│   ├── CMakeLists.txt      ← Build system configuration
│   ├── vendor/sqlite3/     ← SQLite source code (compile yourself)
│   └── src/                ← C++ source files
│
├── README.md
└── BUILDING.md             ← How to compile and run both clients
```

---

## 3. The shared database

### `docs/schema.sql`

**This is the most important file in the entire project.** It defines every table, every column, every index, and every rule that both clients must follow. Think of it as the contract between the two applications — as long as both write data in the format this file describes, they can share data seamlessly.

#### What tables exist

**`categories`** — Broad groupings of medicines (e.g. "Antibiotics", "Vitamins"). Currently a reference list; not heavily used in queries yet.

**`suppliers`** — Pharmaceutical companies or distributors that supply stock to the pharmacy. Stores their GSTIN (GST registration number) and drug licence number, which are legally required on purchase invoices in India.

**`customers`** — People who buy medicines from the shop. Stored with their phone number and optional GSTIN (for businesses buying wholesale — this affects how GST is split on the invoice).

**`products`** — The medicine catalogue. Every medicine the pharmacy could possibly sell has one row here. Important columns:
- `sku` — A short code the pharmacist assigns (e.g. "PARA500" for Paracetamol 500mg)
- `hsn_code` — A government-assigned code for the type of product, used on tax invoices
- `gst_rate` — The GST percentage for this medicine (0%, 5%, 12%, 18%, or 28%)
- `is_scheduled` — Legal classification: 0 = OTC (anyone can buy), 1 = Schedule H (requires prescription), 2 = Schedule H1 (controlled), 3 = Schedule X (narcotic)
- `reorder_level` — When stock falls below this number, the system raises an alert

**`stock_batches`** — The actual physical stock in the shop. One product can have many batches (e.g. two deliveries of the same medicine, with different expiry dates and different purchase prices). Important columns:
- `batch_number` — The batch number printed on the medicine box
- `expiry_date` — Stored as "YYYY-MM" (month precision is enough for pharma)
- `quantity` — How many units are currently in stock
- `purchase_price` — What the pharmacy paid the supplier
- `mrp` — Maximum Retail Price — the price printed on the box that the customer pays (this is set by law in India; you cannot sell above MRP)

**`bills`** — One row per sales transaction. Records the total amounts, payment method, GST breakdown, and whether the bill was later cancelled.

**`bill_items`** — The individual line items within each bill (one row per medicine sold). Stores a snapshot of the price and GST amounts at the time of sale, because prices may change later. This is legally required in India.

**`devices`** — Tracks which POS terminals exist (each terminal gets a unique ID on first launch). Used for the sync system.

**`sync_outbox`** — A queue of database changes waiting to be sent to the cloud. Every time a row in a business table is inserted or updated, a trigger automatically adds an entry here. The sync agent reads from this table and sends the data to a central server.

**`gstr1_exports`** — A log of GST return export operations (when the pharmacist downloads data for filing with the government).

#### Reporting views

The schema also defines three SQL views — these are like saved queries that make reports easier:

- **`v_bill_gst_summary`** — GST breakdown per bill, grouped by HSN code
- **`v_gstr1_b2c`** — Monthly HSN-wise summary for B2C sales (the format required for GSTR-1 Table 7)
- **`v_low_stock`** — Products whose current stock is at or below the reorder level

#### The LWW sync system (Last-Writer-Wins)

Every business table has three extra columns: `updated_at_us`, `device_id`, and `row_version`. These are used when syncing data between multiple terminals (e.g. two pharmacists using two computers in the same shop). If both computers edit the same record, the one with the later timestamp wins. If timestamps are identical (clock skew), the higher version number wins. If those are also equal, the device ID is used as a tiebreaker. This is called **Last-Writer-Wins (LWW)** conflict resolution.

#### Triggers

The schema sets up **AFTER triggers** on the main tables. A trigger is code that runs automatically inside the database engine immediately after an INSERT or UPDATE. Here, each trigger writes a complete JSON snapshot of the changed row into `sync_outbox`. Because the trigger fires inside the same database transaction as the original change, if the transaction is rolled back (e.g. a checkout fails midway), the sync entry is also rolled back — data consistency is guaranteed.

---

## 4. The modern client

The modern client uses **Tauri**, a framework that bundles a Rust backend and a web-based frontend into a single desktop application. The Rust code handles all database operations. The React code handles all visual elements. They communicate through a message-passing system called IPC (Inter-Process Communication).

```
User clicks button
      ↓
React (TypeScript) calls invoke("command_name", args)
      ↓
Tauri IPC channel
      ↓
Rust #[tauri::command] function executes
      ↓
SQLite database on disk
      ↓
Result returned to React as JSON
```

---

### 4.1 The Rust backend

#### `src-tauri/Cargo.toml`

The Rust package configuration file. It lists every external library the Rust code depends on:

- **`tauri`** — The framework that creates the application window and handles IPC
- **`rusqlite`** — Rust bindings to SQLite; compiled with the `bundled` feature so it includes the SQLite source code itself, meaning you don't need SQLite installed separately on the computer
- **`tokio`** — An async runtime; Tauri uses this internally, and the app uses it for background tasks
- **`serde` / `serde_json`** — Serialisation: converts Rust structs to JSON and back (used for IPC messages)
- **`chrono`** — Date and time utilities (formatting bill dates)
- **`uuid`** — Generates the random device ID on first launch
- **`anyhow` / `thiserror`** — Error handling utilities
- **`windows`** — Windows API bindings, used only for the printer module on Windows

The release profile is tuned for a small binary (`opt-level = "z"`, `lto = true`, `strip = true`).

---

#### `src-tauri/build.rs`

A tiny file that runs at compile time. It calls `tauri_build::build()` which generates the code Tauri needs to wire up the IPC handler. Nothing to customise here.

---

#### `src-tauri/tauri.conf.json`

Configures the Tauri application packaging:
- The window title, initial size, and whether it is resizable
- The path to the compiled frontend (the `dist/` folder Vite produces)
- Application identifier and version (used for the installer)
- Which OS features the app is allowed to use (file system, window management, etc.)

---

#### `src-tauri/src/main.rs`

The **entry point** of the Rust application. The first thing to run when the program starts.

What it does, step by step:
1. Figures out where the executable lives on disk and uses that folder as the data directory (this is where `pharmacy.db` and `device.cfg` will live alongside the `.exe`)
2. Calls `load_or_create_device_id()` — reads a UUID from `device.cfg` or generates a new one and saves it. This UUID uniquely identifies this POS terminal across all sync operations
3. Calls `db::open()` to open (or create) the SQLite database file
4. Creates the sync communication channel (`sync_tx` / `sync_rx`) — a pipe that commands can use to tell the sync agent "something just changed, ship it now"
5. Reads the sync endpoint URL from an environment variable (defaults to `https://sync.aushadhalayam.local/api/ingest`)
6. Builds the Tauri application:
   - `.setup()` spawns the background sync agent using Tauri's async runtime
   - `.manage(app_state)` makes the database connection and device ID available to every command handler
   - `.invoke_handler()` registers all the Rust functions that React can call by name

---

#### `src-tauri/src/state.rs`

Defines the `AppState` struct — the shared state that every command handler has access to:

- `db` — The SQLite connection, wrapped in `Arc<Mutex<...>>`. `Arc` means multiple threads can hold a reference to it. `Mutex` means only one thread can use it at a time (SQLite is not thread-safe without this)
- `sync_tx` — A sender for the sync trigger channel; command handlers call this after committing a transaction to tell the sync agent to wake up
- `device_id` — The UUID string; immutable after startup

---

#### `src-tauri/src/db/mod.rs`

The **database initialisation module**. Responsible for:

1. `open()` — Opens the SQLite file with the right flags, then calls `configure()` and `bootstrap_schema()`
2. `configure()` — Runs PRAGMAs to set up WAL journal mode (allows reading while writing), foreign key enforcement, and performance settings (8 MB cache, 5 second timeout when the DB is locked)
3. `bootstrap_schema()` — Reads `docs/schema.sql` directly from the source tree at compile time using `include_str!()` and runs it. Because all statements use `IF NOT EXISTS`, running this on a database that already has the schema is harmless
4. `register_device()` — Inserts this terminal's UUID into the `devices` table with `INSERT OR IGNORE` (does nothing if it already exists)
5. `now_us()` — A helper function used throughout the codebase that returns the current time as an integer number of microseconds since January 1, 1970. Used for the `updated_at_us` LWW timestamp column

---

#### `src-tauri/src/db/sync.rs`

The **background sync agent**. Runs as a background task forever while the app is open.

How it works:
1. `spawn_sync_agent()` starts a Tokio async task that loops until the app shuts down
2. Inside the loop, it waits for either: a signal from a command handler (meaning "new data was just committed"), or a 60-second periodic heartbeat
3. When either fires, it calls `run_sync_cycle()`
4. `run_sync_cycle()` reads up to 50 rows from `sync_outbox` where `is_transmitted = 0` and `retry_count < 5`
5. It sends those rows as a JSON array to the cloud endpoint via HTTP POST
6. On success, it marks the rows `is_transmitted = 1`. On failure, it increments `retry_count` and records the error message. Rows that have failed 5 or more times are skipped (circuit breaker) until a manual reset

The HTTP call currently uses a placeholder (`simulate_http_post`) — replace it with a real `reqwest` call when the cloud endpoint is ready.

---

#### `src-tauri/src/commands/mod.rs`

A simple module index file. Re-exports everything from the three command sub-modules so `main.rs` only needs to write `commands::search_products` instead of `commands::inventory::search_products`.

---

#### `src-tauri/src/commands/inventory.rs`

Handles all commands related to stock and the product catalogue.

**`search_products`** — Called when the pharmacist types in the medicine search box during billing or in the Medicines screen. Takes a query string, searches product `name`, `sku`, and `generic_name` with a SQL `LIKE` pattern. Joins to `stock_batches` to also return the total available quantity. Returns up to 20 results, ordered by available stock descending (so in-stock medicines appear first).

**`get_stock_batches`** — Called after a product is selected, to show which physical batches are available for that medicine. Filters out expired and zero-quantity batches. Returns results ordered by expiry date ascending — this is called **FEFO** (First Expired, First Out) and is a legal requirement in pharmacy: the batch closest to expiry should be sold first to avoid medicines going bad on the shelf.

**`receive_stock`** — Records new stock arriving from a supplier. Uses `INSERT OR CONFLICT DO UPDATE` so that if the same batch number for a product is received again, the quantity is added rather than duplicated.

**`get_low_stock_alerts`** — Queries products where the sum of all non-expired batch quantities is at or below `reorder_level`. Used by the Dashboard and Inventory screens to show alerts.

Each command uses `tokio::task::spawn_blocking()` — this runs the SQLite code on a separate thread pool, freeing the async runtime to handle other tasks while waiting for disk I/O.

---

#### `src-tauri/src/commands/billing.rs`

Handles checkout and bill management — the most critical part of the application.

**`compute_line()`** — A pure function (no database access) that computes all the GST amounts for a single line item. See the [GST section](#indian-gst-calculation) for the formula. This is called once per item before any database writes happen, so the totals are known before the transaction begins.

**`process_checkout`** — The checkout command. What happens when the pharmacist clicks "Checkout":

1. Validates the cart is not empty
2. Calls `compute_line()` for every item to get all the GST amounts
3. Sums up totals (taxable value, CGST, SGST, IGST, grand total)
4. Opens a `BEGIN IMMEDIATE` transaction — this locks the database so no other terminal can sell the same stock at the same time (prevents overselling)
5. For each item, queries the current stock quantity and verifies there is enough. If any item fails this check, the transaction is abandoned (the Rust `?` operator causes an early return, and the RAII transaction guard automatically rolls back)
6. Deducts the quantity from each stock batch and updates the LWW columns
7. Generates a bill number in the format `BILL-YYYYMMDD-NNNN`
8. Inserts the bill header row
9. Inserts one row per line item into `bill_items`
10. Commits the transaction — at this moment, the SQLite triggers fire and write sync_outbox entries for the bills and bill_items rows
11. Sends a sync trigger signal

**`cancel_bill`** — Reverses a bill. Restores stock quantities for each line item, then marks the bill `is_cancelled = 1`.

**`get_pending_sync_count`** — Returns how many rows in `sync_outbox` have not yet been transmitted. Shown in the top bar of the UI.

The module also contains **unit tests** for `compute_line()` verifying the GST arithmetic at 12% intrastate, 18% interstate, and with a discount applied.

---

#### `src-tauri/src/commands/gst.rs`

Handles GST reporting commands used by the Sales screen.

**`get_gstr1_b2c_summary`** — Queries the `v_gstr1_b2c` view to return a monthly HSN-wise tax summary. This is the data pharmacists need to fill out Table 7 of their GSTR-1 return on the government's GST portal. Takes a period in `MMYYYY` format (e.g. "062026" for June 2026).

**`get_period_summary`** — Returns aggregate totals for a month: number of bills, total taxable value, total CGST, SGST, IGST, and grand total. Used by the Dashboard and Sales screen.

**`get_gstr1_bill_details`** — Returns a row-per-bill breakdown from the `v_bill_gst_summary` view, for auditing individual invoices within a period.

---

#### `src-tauri/src/printer/mod.rs`

Receipt printing. Supports three modes selected by configuration:

**GDI Spooler (Windows)** — Routes through the Windows Print Spooler using Win32 API calls (`CreateDC`, `StartDoc`, `TextOut`, `EndDoc`). Works with any printer driver installed in Windows — useful for shops that have a regular laser printer or a USB receipt printer with a Windows driver installed.

**ESC/POS over COM port (Windows)** — Sends raw ESC/POS commands directly to a serial (COM) port. ESC/POS is the command language used by thermal receipt printers (Epson TM series, etc.). This is the fastest path — no Windows driver needed, the printer just receives binary commands and prints immediately.

**ESC/POS over TCP (Android)** — Same ESC/POS commands but sent over a Wi-Fi network connection. Used for wireless thermal printers on Android tablets.

`build_escpos()` — Builds the binary ESC/POS byte sequence for a receipt. Uses ESC/POS escape codes to: initialise the printer, centre-align the pharmacy name, bold it, print a double-width header, format the line items table, print totals, and issue a paper-cut command.

---

### 4.2 The React frontend

The frontend is a standard React + TypeScript + Vite application. Vite compiles all the TypeScript and CSS into plain HTML/JS/CSS files in the `dist/` folder. Tauri then serves that folder as the app's user interface.

#### `src/main.tsx`

The single entry point that mounts the React app into the HTML page.

---

#### `src/App.tsx`

The **root component** — the outermost piece of UI that wraps everything else.

Responsibilities:
- Tracks which screen is currently shown (`screen` state variable)
- Tracks the colour theme and whether animations are enabled, and persists these to `localStorage` so they survive app restarts
- Creates an `AppContext` (a React context) that any component in the tree can read, to get/set the current screen and theme — this avoids having to pass these values down through many layers of components ("prop drilling")
- Renders the fixed layout: `<Sidebar />` on the left, and the main area on the right with `<Topbar />` at the top and the current screen's component below it

---

#### `src/components/Sidebar.tsx`

The left navigation panel. Renders a vertical list of navigation buttons — one per screen. Highlights the active screen. When clicked, calls `setScreen()` from `AppContext` to switch screens.

Groups the navigation items into labelled sections ("Main", "Store", "Reports", "System") for readability.

---

#### `src/components/Topbar.tsx`

The thin bar at the top of the main area. Shows:
- The name of the current screen
- A sync status indicator (green dot = synced, pulsing dot = X rows pending). Polls `get_pending_sync_count` every 15 seconds
- Today's date in Indian locale format

---

#### `src/data/types.ts`

TypeScript interface definitions for every data structure the frontend works with. These **must exactly match** the fields and types that Rust's `serde` library serialises. If a Rust struct has `pub id: i64`, the TypeScript interface must have `id: number` (not `string`). If a field is `Option<String>` in Rust, it must be `string | null` in TypeScript.

Key types:
- `Product` — a medicine from the catalogue
- `StockBatch` — a physical batch of stock for a product
- `BillItem` — one line on a bill being built (includes the available batches so the user can switch which batch is being sold)
- `LowStockAlert` — a product that has run low
- `Customer` — a registered customer
- `PeriodSummary` — monthly totals for the dashboard
- `Gstr1B2cRow` — one row of the GSTR-1 HSN summary

---

#### `src/data/api.ts`

The **data access layer** — the only file in the frontend that knows how to talk to the Rust backend. All screens import from here; none of them call `invoke` directly.

Every function follows the same pattern:
```typescript
export async function someFunction(arg: SomeType): Promise<SomeResult> {
  try {
    return await invoke<SomeResult>("rust_command_name", { arg });
  } catch {
    return fallbackMockData;  // used during browser-only development
  }
}
```

The `try/catch` fallback means the frontend works in a normal browser (via `npm run dev`) even without the Rust backend running — it just returns mock data instead. When running inside Tauri, `invoke` succeeds and real data is returned.

Functions defined here:
- `searchProducts(query)` — search the medicine catalogue
- `getStockBatches(productId)` — get available batches for a product
- `getLowStockAlerts()` — get low-stock items
- `processCheckout(payload)` — submit a completed bill
- `getPendingSyncCount()` — how many sync rows are waiting
- `getPeriodSummary(month)` — dashboard totals
- `getGstr1B2cSummary(month)` — GSTR-1 data
- `searchCustomers(query)` / `getCustomers()` — customer lookup (currently mock-only)

Also exports the `CheckoutPayload` and `BillResult` TypeScript interfaces, which mirror the Rust `CheckoutRequest` and `BillResult` structs exactly.

---

#### `src/data/mock.ts`

Sample data used when running in a browser without Tauri. Contains realistic Indian pharmacy data (Paracetamol, Amoxicillin, Alprazolam, etc.) with correct GST rates, HSN codes, and schedule classifications. The field names are identical to what the Rust backend returns, so the same screen components work with both real and mock data.

---

#### `src/tabler-icons.d.ts`

A TypeScript declaration file that tells the TypeScript compiler the types for the `@tabler/icons-react` icon library. The installed version of this package is missing its own type declaration files, so this stub provides them manually. Lists every icon actually used in the project.

---

#### `src/screens/Dashboard.tsx`

The home screen. Shows four KPI cards (total sales this month, total bills, low stock count, average daily sales) and two panels: a simple bar chart of the last 7 days' sales, and a table of low-stock alerts.

Clicking "View alerts" or "View all" navigates to the Inventory screen. The Quick Actions buttons at the bottom navigate to other screens.

---

#### `src/screens/Billing.tsx`

The main billing workflow. This is the most complex screen.

Left panel — the bill:
- A medicine search bar with typeahead. As the user types, `searchProducts()` is called with a 200ms debounce. Selecting a medicine calls `getStockBatches()` and adds the first non-expired, in-stock batch to the bill automatically (FEFO)
- A table of items on the current bill. Each row lets the pharmacist change the batch (via dropdown), adjust quantity, and apply a line discount
- If any scheduled medicine (H, H1, or X) is on the bill, a red panel appears requiring the doctor's name and prescription number before checkout is allowed

Right panel — the bill summary:
- Customer search with typeahead (optional)
- Notes field
- Subtotal, GST collected, and grand total
- Payment mode buttons (CASH, UPI, CARD, CREDIT, CHEQUE)
- The Checkout button, disabled if the cart is empty or a required doctor name is missing

On successful checkout, the bill number is shown in a green success banner.

---

#### `src/screens/Inventory.tsx`

Shows two things:
1. A warning banner at the top if any products are below reorder level (with badges listing the affected medicines)
2. A table of all stock batches — medicine name, batch number, expiry date (colour-coded: red = expired, yellow = expiring within 90 days, green = fine), MRP, purchase price, quantity, and calculated profit margin

Currently uses mock data from `mock.ts` for the batch table (the `getInventory` backend command is not yet wired up in the frontend).

---

#### `src/screens/Medicines.tsx`

The medicine catalogue browser. Shows a search box and a table of all products with their generic name, manufacturer, schedule classification (shown as a coloured pill badge — OTC/Sch H/Sch H1/Sch X), HSN code, GST rate, reorder level, and unit. The search is debounced and calls `searchProducts()` live as the user types.

The "Add Medicine" button exists but the CRUD form is not yet implemented.

---

#### `src/screens/Purchase.tsx`

A preview of the purchase order screen. Shows existing stock batches as a history of received stock (using mock data). Has a collapsible form for recording a new stock receipt (UI only — the Save button does not yet call the backend `receive_stock` command). This screen is labelled "UI preview — backend coming soon".

---

#### `src/screens/Sales.tsx`

The sales reports and GSTR-1 screen. The user picks a month with a month picker. The screen then shows:
1. Four KPI cards (total sales, number of bills, total GST collected, average bill value)
2. The GSTR-1 B2C HSN-wise summary table — one row per HSN code, showing taxable value and CGST/SGST/IGST split
3. A daily sales table for the selected month

The Export CSV button is present but not yet connected to an export function.

---

#### `src/screens/Customers.tsx`

A searchable list of registered customers. Filters locally by name or phone as the user types. Shows whether each customer is B2B (has a GSTIN) or B2C. The "Add Customer" button and backend CRUD are not yet implemented.

---

#### `src/screens/Settings.tsx`

A tabbed settings panel with five tabs:

- **Appearance** — Colour theme picker (Blue, Dark, Emerald, Rose, Violet) and an animations toggle. These settings are saved to `localStorage` immediately and applied to the whole app via a `data-theme` attribute on `<html>`
- **Billing** — Toggles for auto-print, customer prompt, expiry warning, and a default discount percentage. Not yet connected to persistent storage
- **Sync** — Toggle sync on/off, set the endpoint URL, set the sync interval. Not yet connected to backend
- **Hardware** — Printer port and paper width selection. Not yet connected to `PrinterManager`
- **GST / Tax** — GSTIN, state code, legal business name, and address. Not yet persisted

---

## 5. The legacy client

The legacy client is a traditional Windows desktop application written in C++11 using the wxWidgets UI toolkit. It compiles to a 32-bit Windows executable that runs on Windows XP SP3 through Windows 11. The entire application is a single executable with no external DLL dependencies (statically linked).

---

#### `CMakeLists.txt`

The build system configuration. Uses CMake to:
- Compile all the `.cpp` files listed in `APP_SOURCES` into a single executable
- Link against wxWidgets, the SQLite static library from `vendor/sqlite3/`, and Windows system libraries
- Set linker flags to produce a native Windows GUI application (no console window)
- Strip debug symbols in release builds

---

#### `src/main.cpp`

The entry point. Defines a `PosApp` class that inherits from `wxApp`. wxWidgets calls `PosApp::OnInit()` at startup, which:
1. Finds the folder where the executable lives
2. Reads or generates the device UUID (stored in `device.cfg` using `wxFileConfig`)
3. Reads the printer configuration from `printer.cfg`
4. Creates a `MainFrame` window (the main application window) and shows it

---

#### `src/MainFrame.h` / `MainFrame.cpp`

The main application window. In the legacy client, `MainFrame` serves a similar role to `App.tsx` in the modern client — it is the root of the UI.

Responsibilities:
- Creates and owns the `PharmacyWorkerThread` (the background database thread)
- Builds the overall layout (sidebar, page area)
- Handles navigation between pages
- Receives results from the worker thread via `wxCommandEvent` messages and routes them to the right page component
- Stores "pending request" state for multi-step operations (e.g. searching for a product requires two database calls: first fetch product details, then fetch its batches)

---

#### `src/WorkerThread.h` / `src/WorkerThread.cpp`

The **database backend** for the legacy client. Equivalent in role to the `src-tauri/src/commands/` folder in the modern client.

The architecture separates the GUI thread from database operations completely:

```
GUI Thread (MainFrame)                Worker Thread (PharmacyWorkerThread)
────────────────────────              ────────────────────────────────────
PostTask(DbTask{type, json})  ──►     mutex-protected queue
                                      Entry() loop reads task
                                      executes SQL
                              ◄──     wxQueueEvent(result JSON)
OnDbResult(event)
```

The worker thread owns the SQLite connection for its entire lifetime. The GUI thread never touches the database directly. Communication in both directions uses JSON strings — this means the interface between the GUI and the database is self-contained and language-neutral.

**Command handlers** (each takes a JSON string of parameters and returns a JSON string result):

- `HandleSearchProduct` — equivalent to `search_products` in Rust
- `HandleGetStockBatch` — equivalent to `get_stock_batches` in Rust
- `HandleCheckout` — equivalent to `process_checkout` in Rust. Because the items array is nested JSON, this handler includes a simple JSON array parser (finding `{` and `}` characters to split items)
- `HandleCancelBill` — equivalent to `cancel_bill` in Rust
- `HandleFlushSync` — equivalent to `get_pending_sync_count` in Rust
- `HandleGetLowStock` — equivalent to `get_low_stock_alerts` in Rust
- `HandleSearchCustomer` — customer search by name or phone
- `HandleGetInventory` — full batch list with margin calculation
- `HandleSearchMedicine` — delegates to `HandleSearchProduct`
- `HandleGetPeriodSales` — daily sales totals for a month

The file also contains **`jsonutil`** — a small namespace with hand-written JSON building and parsing functions. These exist because the legacy client avoids external dependencies. The modern client uses `serde_json` for the same purpose.

**`now_us()`** is also implemented here using the Windows `GetSystemTimeAsFileTime` API, which is available from Windows XP onwards. It produces the same microsecond-since-epoch timestamp as the Rust version.

---

#### `src/DatabaseManager.h` / `src/DatabaseManager.cpp`

A RAII (Resource Acquisition Is Initialisation) wrapper around the raw SQLite C API. Makes it safer to use SQLite by:
- `SqliteStmt` — Automatically finalises a prepared statement when it goes out of scope, preventing resource leaks
- `DbRow` — A simple key-value bag for query results (column name → string value)
- `DatabaseManager` — Manages the `sqlite3*` connection handle; provides typed methods for `exec`, `exec_stmt`, `query`, `begin_immediate`, `commit`, and `rollback`

Note: `WorkerThread.cpp` defines its own inline `Stmt` struct (similar to `SqliteStmt`) and uses the raw SQLite API directly for performance. The `DatabaseManager` class is available but is not currently used in the main code path.

---

#### `src/GstEngine.h`

A **header-only** C++ implementation of the Indian GST calculation rules for pharmacy retail. Contains the same business logic as `billing.rs::compute_line()` in the modern client.

Key types and functions:
- `LineResult` — Holds all computed values for one bill line (unit price, line total, taxable value, CGST, SGST, IGST)
- `BillTotals` — Aggregate totals across all lines
- `round2(v)` — Rounds a decimal to 2 places using standard Indian accounting rounding (round half up)
- `compute_line(mrp, qty, discount_pct, gst_rate, is_interstate)` — Computes all amounts for one line item
- `is_valid_gst_rate(rate)` — Checks if a rate is one of the legally recognised Indian GST slabs
- `hsn_description(hsn)` — Returns a human-readable category for an HSN code (e.g. HSN starting with "30" → "Pharmaceutical Products")

---

#### `src/PrinterManager.h` / `src/PrinterManager.cpp`

Equivalent to `src-tauri/src/printer/mod.rs` in the modern client. Supports two print modes:
- `GDI_SPOOLER` — Windows Print Spooler via `CreateDC` / `TextOut` / `EndDoc`
- `ESCPOS_COM` — Raw ESC/POS bytes to a COM serial port

Configuration is loaded from `printer.cfg` at startup by `main.cpp`.

---

#### `src/BillingGrid.h` / `src/BillingGrid.cpp`

A virtual grid model for displaying the current bill items in a wxGrid widget. Instead of storing all rows in memory and giving wxGrid a full copy, a virtual model (`wxGridTableBase`) tells wxGrid how many rows there are and provides cell values on demand. This is more efficient for large bills. The grid displays: medicine name, batch, expiry, quantity, discount, GST rate, unit price, and line total.

---

#### `src/*Page.h` / `src/*Page.cpp` (DashboardPage, BillingPage, InventoryPage, etc.)

These files contain the page components for each section of the legacy UI, analogous to the screen components in `src/screens/` on the modern client. Each page is a `wxPanel` subclass that builds its own layout and widgets. Navigation is handled by `MainFrame`, which shows/hides the appropriate page panel.

---

#### `src/SidebarPanel.h` / `src/SidebarPanel.cpp`

The left navigation panel, equivalent to `Sidebar.tsx`. Renders navigation buttons using wxWidgets and posts navigation events to `MainFrame` when clicked.

---

#### `src/ChartPanel.h` / `src/ChartPanel.cpp`

A custom wxPanel that draws a bar chart of daily sales using wxWidgets GDI drawing primitives (`wxDC::DrawRectangle`). Equivalent to the `<BarChart>` component in `Dashboard.tsx`.

---

#### `src/AppColors.h`

A header defining the colour palette used throughout the legacy UI — primary blue, success green, warning amber, danger red, and neutral grays. Equivalent to the CSS custom properties in the modern client's stylesheet.

---

#### `src/Pages.h`

A shared header that forward-declares all the page classes and any types or event IDs they share. Keeps inter-page includes clean.

---

## 6. How the two clients share data

Both clients use the same `pharmacy.db` SQLite file format and the same schema. If a pharmacy has two computers in one shop, the data flow looks like this:

```
Computer A (legacy client)          Computer B (modern client)
pharmacy.db                         pharmacy.db
      │                                   │
      │  After every commit:              │  After every commit:
      │  triggers write to                │  triggers write to
      │  sync_outbox                      │  sync_outbox
      │                                   │
      └──────────────► Cloud PostgreSQL ◄─┘
                       (merge layer,
                        LWW conflict
                        resolution)
                            │
                    ◄───────┴───────►
                 Sync back to both clients
                 on next sync cycle
```

The sync agent in each client periodically reads from its local `sync_outbox`, sends the data to the cloud, and the cloud applies Last-Writer-Wins logic to merge changes from all terminals.

---

## 7. Key concepts explained

### Indian GST calculation

In India, the price printed on a medicine box (MRP — Maximum Retail Price) already **includes** GST. This is different from many countries where tax is added on top at checkout.

So to calculate the tax collected on a sale, you must work backwards:

```
line_total    = mrp × quantity × (1 − discount% / 100)
taxable_value = line_total ÷ (1 + gst_rate / 100)
gst_amount    = line_total − taxable_value
```

For example, a ₹112 medicine at 12% GST:
- `taxable_value = 112 ÷ 1.12 = ₹100`
- `gst_amount = ₹112 − ₹100 = ₹12`
- CGST = ₹6, SGST = ₹6 (split equally for sales within the same state)
- For interstate sales: IGST = ₹12, CGST = 0, SGST = 0

### FEFO (First Expired, First Out)

A legal and practical rule in pharmacy: when you have the same medicine in multiple batches with different expiry dates, you must sell the batch closest to expiry first. This prevents medicines from sitting on the shelf until they expire.

In the code, this is implemented by sorting batches with `ORDER BY expiry_date ASC` in every stock query. The frontend automatically picks the first (earliest-expiry) batch when adding a medicine to a bill.

### LWW (Last-Writer-Wins) sync

When multiple POS terminals can all edit the same data, conflicts can arise: Terminal A and Terminal B both update the same product's price at the same time. LWW resolves this simply: whoever saved their change later (higher `updated_at_us` timestamp) wins. The loser's change is discarded at the cloud merge layer. This is simple and predictable, though it means a slightly older change can be lost — acceptable for a pharmacy where two terminals editing the same product simultaneously is rare.

### Sync outbox pattern

Rather than syncing directly to the cloud on every write (which would fail if the internet is down and slow down checkouts), the app writes changes to a local `sync_outbox` table first. A background agent then sends these queued changes to the cloud when connectivity is available. This means the POS works normally even with no internet, and data is eventually consistent once connectivity is restored.

### Scheduled medicines

Indian law classifies medicines into schedules that control how they may be sold:
- **OTC (Schedule 0)** — Over the counter, anyone can buy
- **Schedule H** — Requires a doctor's prescription
- **Schedule H1** — Stricter controlled medicines (specific antibiotics, some antidepressants); requires prescription and the pharmacy must maintain a register
- **Schedule X** — Narcotics and psychotropics; most strictly controlled

The app enforces that a doctor's name must be recorded before a bill containing Schedule H, H1, or X medicines can be checked out.

### WAL mode (Write-Ahead Logging)

SQLite's default behaviour locks the entire database file during any write, preventing all reads at the same time. WAL mode changes this so reads can happen concurrently with writes. This is important for the POS because the background sync agent reads `sync_outbox` at the same time the billing code is writing to `bills`.

### `include_str!()` and compile-time embedding

Rust's `include_str!("path/to/file")` macro reads a text file at compile time and embeds its contents as a string literal inside the compiled binary. This is used to include `docs/schema.sql` in the Rust binary — when the binary runs on a customer's computer, the schema SQL is already inside the `.exe` file; no separate `schema.sql` file needs to be shipped.
