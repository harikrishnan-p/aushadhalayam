// =============================================================================
// main.rs  —  Tauri 2 application entry point for Aushadhalayam POS
// =============================================================================

// Prevent a console window from opening on Windows in release builds
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod commands;
mod db;
mod printer;
mod state;

use state::{AppState, SyncTrigger};
use std::path::PathBuf;
use tokio::sync::mpsc;

// ─────────────────────────────────────────────────────────────────────────────
// Determine or create the device UUID
// ─────────────────────────────────────────────────────────────────────────────
fn load_or_create_device_id(data_dir: &PathBuf) -> String {
    let cfg_path = data_dir.join("device.cfg");

    if let Ok(content) = std::fs::read_to_string(&cfg_path) {
        let trimmed = content.trim().to_string();
        if !trimmed.is_empty() {
            return trimmed;
        }
    }

    let id = uuid::Uuid::new_v4().to_string();
    let _ = std::fs::write(&cfg_path, &id);
    id
}

// ─────────────────────────────────────────────────────────────────────────────
// Resolve the local data directory (adjacent to the executable)
// ─────────────────────────────────────────────────────────────────────────────
fn data_dir() -> PathBuf {
    std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|d| d.to_path_buf()))
        .unwrap_or_else(|| PathBuf::from("."))
}

// ─────────────────────────────────────────────────────────────────────────────
// Application entry point
// ─────────────────────────────────────────────────────────────────────────────
fn main() {
    let dir       = data_dir();
    let db_path   = dir.join("pharmacy.db");
    let device_id = load_or_create_device_id(&dir);

    // Open SQLite connection (WAL configured inside db::open)
    let conn = db::open(
        db_path.to_str().expect("DB path contains non-UTF-8 chars"),
        &device_id,
    )
    .expect("Cannot open database");

    // The sender is stored in AppState; the receiver is consumed by the sync loop.
    let (sync_tx, sync_rx) = mpsc::unbounded_channel::<SyncTrigger>();

    let app_state = AppState::new(conn, sync_tx, device_id);
    let db_arc    = app_state.db.clone();

    let sync_endpoint = std::env::var("SYNC_ENDPOINT")
        .unwrap_or_else(|_| "https://sync.aushadhalayam.local/api/ingest".to_string());

    // ── Tauri application ────────────────────────────────────────────────────
    tauri::Builder::default()
        .setup(move |_app| {
            // Tauri owns the async runtime at this point.
            // spawn_sync_agent calls tokio::spawn, which uses Tauri's runtime.
            db::sync::spawn_sync_agent(db_arc, sync_rx, sync_endpoint);
            Ok(())
        })
        .manage(app_state)
        .invoke_handler(tauri::generate_handler![
            // Inventory commands
            commands::search_products,
            commands::get_stock_batches,
            commands::receive_stock,
            commands::get_low_stock_alerts,
            // Billing commands
            commands::process_checkout,
            commands::cancel_bill,
            commands::get_pending_sync_count,
            // GST commands
            commands::get_gstr1_b2c_summary,
            commands::get_period_summary,
            commands::get_gstr1_bill_details,
        ])
        .run(tauri::generate_context!())
        .expect("Error while running Aushadhalayam POS");
}
