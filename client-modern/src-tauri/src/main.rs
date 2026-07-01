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
use tauri::Manager;
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
// Application entry point
// ─────────────────────────────────────────────────────────────────────────────
fn main() {
    // In release builds the console is hidden, so route panics to a log file
    // next to the exe so crashes are diagnosable without a debugger.
    #[cfg(not(debug_assertions))]
    {
        let log_path = std::env::current_exe()
            .ok()
            .and_then(|p| p.parent().map(|d| d.join("crash.log")))
            .unwrap_or_else(|| std::path::PathBuf::from("crash.log"));
        std::panic::set_hook(Box::new(move |info| {
            let msg = format!("{info}\n");
            let _ = std::fs::write(&log_path, &msg);
        }));
    }

    // ── Tauri application ────────────────────────────────────────────────────
    // NOTE: db path and device_id are resolved inside .setup() so we have
    // access to Tauri's app_data_dir() — the correct writable location on
    // every platform (%APPDATA%\com.aushadhalayam.pos on Windows).
    tauri::Builder::default()
        .setup(move |app| {
            let dir = app
                .path()
                .app_data_dir()
                .expect("Cannot resolve app data directory");
            std::fs::create_dir_all(&dir).expect("Cannot create app data directory");

            let db_path   = dir.join("pharmacy.db");
            let device_id = load_or_create_device_id(&dir);

            let conn = db::open(
                db_path.to_str().expect("DB path contains non-UTF-8 chars"),
                &device_id,
            )
            .expect("Cannot open database");

            let sync_endpoint = std::env::var("SYNC_ENDPOINT")
                .unwrap_or_else(|_| "https://sync.aushadhalayam.local/api/ingest".to_string());

            let (sync_tx, sync_rx) = mpsc::unbounded_channel::<SyncTrigger>();
            let app_state = AppState::new(conn, sync_tx, device_id);
            let db_arc    = app_state.db.clone();

            db::sync::spawn_sync_agent(db_arc, sync_rx, sync_endpoint);
            app.manage(app_state);
            Ok(())
        })
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
