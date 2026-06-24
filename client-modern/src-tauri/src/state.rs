// =============================================================================
// state.rs  —  Shared application state injected into every Tauri command
// =============================================================================

use rusqlite::Connection;
use std::sync::{Arc, Mutex};
use tokio::sync::mpsc::UnboundedSender;

/// A unit of work queued for the background sync agent.
#[derive(Debug, Clone)]
pub struct SyncTrigger;

/// Central state object managed by Tauri's state system.
///
/// Thread-safety contract:
///   - `db`       — wrapped in `std::sync::Mutex` so only one thread holds the
///                  connection at a time.  SQLite in NOMUTEX mode is not thread-safe
///                  on its own; the `Mutex` serialises access.
///   - `sync_tx`  — channel sender; cheap to clone; used to kick the sync agent.
///   - `device_id`— immutable after creation; no mutex needed.
pub struct AppState {
    pub db:        Arc<Mutex<Connection>>,
    pub sync_tx:   UnboundedSender<SyncTrigger>,
    pub device_id: String,
}

impl AppState {
    pub fn new(
        db:        Connection,
        sync_tx:   UnboundedSender<SyncTrigger>,
        device_id: String,
    ) -> Self {
        Self {
            db:        Arc::new(Mutex::new(db)),
            sync_tx,
            device_id,
        }
    }
}
