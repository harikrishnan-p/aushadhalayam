// =============================================================================
// db/mod.rs  —  SQLite initialisation, PRAGMA configuration, schema bootstrap
// =============================================================================

pub mod sync;

use anyhow::{Context, Result};
use rusqlite::{Connection, OpenFlags};

/// Open a WAL-mode SQLite connection suited for a single app instance.
/// `path` is the absolute path to the .db file.
pub fn open(path: &str, device_id: &str) -> Result<Connection> {
    let conn = Connection::open_with_flags(
        path,
        OpenFlags::SQLITE_OPEN_READ_WRITE
            | OpenFlags::SQLITE_OPEN_CREATE
            | OpenFlags::SQLITE_OPEN_NO_MUTEX,  // we serialise via Mutex<Connection>
    )
    .context("Cannot open SQLite database")?;

    configure(&conn)?;
    bootstrap_schema(&conn)?;
    register_device(&conn, device_id)?;
    Ok(conn)
}

/// Apply PRAGMAs.  Called once after every connection open.
fn configure(conn: &Connection) -> Result<()> {
    conn.execute_batch(
        "PRAGMA journal_mode  = WAL;
         PRAGMA synchronous   = NORMAL;
         PRAGMA foreign_keys  = ON;
         PRAGMA temp_store    = MEMORY;
         PRAGMA cache_size    = -8000;
         PRAGMA busy_timeout  = 5000;",
    )
    .context("PRAGMA configuration failed")?;
    Ok(())
}

/// Apply the ground-truth schema (all statements are idempotent — IF NOT EXISTS).
/// In production, embed the SQL via include_str! so it ships in the binary.
fn bootstrap_schema(conn: &Connection) -> Result<()> {
    // Embed schema.sql at compile time from the shared docs/ directory.
    // Path is relative to this file's crate root (src-tauri/).
    // The symlink or copy of docs/schema.sql must live at src-tauri/schema.sql.
    let schema = include_str!("../../schema.sql");
    conn.execute_batch(schema)
        .context("Schema bootstrap failed")?;
    Ok(())
}

fn register_device(conn: &Connection, device_id: &str) -> Result<()> {
    let name = format!("POS-{}", &device_id[..device_id.len().min(8)]);
    conn.execute(
        "INSERT OR IGNORE INTO devices(device_id, device_name) VALUES(?1, ?2);",
        rusqlite::params![device_id, name],
    )
    .context("Device registration failed")?;
    Ok(())
}

/// Return current microseconds since Unix epoch (used for LWW columns).
pub fn now_us() -> i64 {
    use std::time::{SystemTime, UNIX_EPOCH};
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_micros() as i64)
        .unwrap_or(0)
}
