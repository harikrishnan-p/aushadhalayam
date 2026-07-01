// =============================================================================
// db/sync.rs  —  Background sync agent: ships sync_outbox rows to the cloud
//
// Architecture
// ─────────────
//   AppState.sync_tx  ──(UnboundedSender<SyncTrigger>)──►  sync_loop()
//
// sync_loop() is a long-running Tokio task.  It waits for a SyncTrigger on
// its channel (or a 60-second periodic wake-up), then drains all unsent rows
// from sync_outbox and POSTs them to the cloud endpoint in batches.
//
// On success  → SET is_transmitted=1, transmitted_at_us=now
// On failure  → SET retry_count = retry_count + 1, error_message = <err>
// Circuit breaker → rows with retry_count >= 5 are skipped until manual reset
// =============================================================================

use crate::state::SyncTrigger;
use anyhow::Result;
use rusqlite::Connection;
use std::sync::{Arc, Mutex};
use std::time::Duration;
use tokio::sync::mpsc::UnboundedReceiver;
use tokio::time;

const MAX_RETRIES:    i32 = 5;
const BATCH_SIZE:     usize = 50;
const POLL_INTERVAL:  Duration = Duration::from_secs(60);

/// Spawn the sync loop as a detached Tokio task.
pub fn spawn_sync_agent(
    db:  Arc<Mutex<Connection>>,
    mut rx: UnboundedReceiver<SyncTrigger>,
    endpoint: String,
) {
    // tauri::async_runtime::spawn works from any thread (including the setup
    // callback on the main thread).  tokio::spawn panics outside a runtime.
    tauri::async_runtime::spawn(async move {
        let mut interval = time::interval(POLL_INTERVAL);
        interval.set_missed_tick_behavior(time::MissedTickBehavior::Skip);

        loop {
            tokio::select! {
                // Either a manual kick from a command handler...
                trigger = rx.recv() => {
                    if trigger.is_none() {
                        // sender dropped — application is shutting down
                        log::info!("Sync agent: channel closed, exiting");
                        break;
                    }
                }
                // ...or the periodic heartbeat
                _ = interval.tick() => {}
            }

            if let Err(e) = run_sync_cycle(&db, &endpoint).await {
                log::error!("Sync cycle error: {e}");
            }
        }
    });
}

/// One sync cycle: read pending rows, POST to cloud, mark transmitted.
async fn run_sync_cycle(
    db:       &Arc<Mutex<Connection>>,
    endpoint: &str,
) -> Result<()> {
    // ── Step 1: collect pending rows (blocking SQLite read) ──────────────────
    let rows = {
        let guard = db.lock().expect("db lock poisoned");
        fetch_pending_rows(&guard)?
    };

    if rows.is_empty() {
        log::debug!("Sync: nothing pending");
        return Ok(());
    }

    log::info!("Sync: shipping {} rows", rows.len());

    // ── Step 2: batch POST to cloud (non-blocking HTTP) ──────────────────────
    // Use reqwest if available, or platform HTTP.  Here we sketch the contract;
    // add `reqwest = { version = "0.12", features = ["json"] }` to Cargo.toml.
    for chunk in rows.chunks(BATCH_SIZE) {
        let payload = serde_json::to_string(chunk)?;

        // Simulated HTTP call — replace with real reqwest call:
        // let resp = reqwest::Client::new()
        //     .post(endpoint)
        //     .header("Content-Type", "application/json")
        //     .body(payload.clone())
        //     .send()
        //     .await;

        // For now, always succeed (replace with real logic)
        let success = simulate_http_post(endpoint, &payload).await;

        // ── Step 3: update outbox in a batch ─────────────────────────────────
        let guard = db.lock().expect("db lock poisoned");
        for row in chunk {
            if success {
                mark_transmitted(&guard, row.id)?;
            } else {
                increment_retry(&guard, row.id, "HTTP POST failed")?;
            }
        }
    }

    Ok(())
}

async fn simulate_http_post(_endpoint: &str, _payload: &str) -> bool {
    // Placeholder — replace with reqwest::Client HTTP call
    tokio::time::sleep(Duration::from_millis(10)).await;
    true
}

// ─────────────────────────────────────────────────────────────────────────────
// SQLite helpers (always called under the Mutex<Connection> guard)
// ─────────────────────────────────────────────────────────────────────────────

#[derive(Debug, serde::Serialize)]
pub struct OutboxRow {
    pub id:         i64,
    pub table_name: String,
    pub operation:  String,
    pub row_pk:     String,
    pub payload:    String,
    pub created_at_us: i64,
    pub device_id:  String,
}

fn fetch_pending_rows(conn: &Connection) -> Result<Vec<OutboxRow>> {
    let mut stmt = conn.prepare_cached(
        "SELECT id, table_name, operation, row_pk, payload, created_at_us, device_id
           FROM sync_outbox
          WHERE is_transmitted = 0 AND retry_count < ?1
          ORDER BY created_at_us ASC
          LIMIT ?2;",
    )?;

    let rows = stmt.query_map(
        rusqlite::params![MAX_RETRIES, BATCH_SIZE as i64],
        |r| Ok(OutboxRow {
            id:           r.get(0)?,
            table_name:   r.get(1)?,
            operation:    r.get(2)?,
            row_pk:       r.get(3)?,
            payload:      r.get(4)?,
            created_at_us:r.get(5)?,
            device_id:    r.get(6)?,
        }),
    )?
    .collect::<Result<Vec<_>, _>>()?;

    Ok(rows)
}

fn mark_transmitted(conn: &Connection, id: i64) -> Result<()> {
    use crate::db::now_us;
    conn.execute(
        "UPDATE sync_outbox
            SET is_transmitted=1, transmitted_at_us=?1
          WHERE id=?2;",
        rusqlite::params![now_us(), id],
    )?;
    Ok(())
}

fn increment_retry(conn: &Connection, id: i64, error: &str) -> Result<()> {
    conn.execute(
        "UPDATE sync_outbox
            SET retry_count  = retry_count + 1,
                error_message = ?1
          WHERE id = ?2;",
        rusqlite::params![error, id],
    )?;
    Ok(())
}
