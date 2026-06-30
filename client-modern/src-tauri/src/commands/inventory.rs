// =============================================================================
// commands/inventory.rs  —  Inventory & stock Tauri commands
// =============================================================================

use crate::db::now_us;
use crate::state::AppState;
use serde::{Deserialize, Serialize};
use tauri::State;

// ─────────────────────────────────────────────────────────────────────────────
// DTOs
// ─────────────────────────────────────────────────────────────────────────────

#[derive(Debug, Serialize)]
pub struct ProductSearchResult {
    pub id:            i64,
    pub sku:           String,
    pub name:          String,
    pub generic_name:  Option<String>,
    pub manufacturer:  String,
    pub hsn_code:      String,
    pub gst_rate:      f64,
    pub unit:          String,
    pub mrp:           f64,
    pub is_scheduled:  i32,
    pub reorder_level: i64,
    pub total_qty:     i64,
}

#[derive(Debug, Serialize)]
pub struct StockBatch {
    pub id:            i64,
    pub batch_number:  String,
    pub expiry_date:   String,
    pub quantity:      i64,
    pub mrp:           f64,
    pub purchase_price:f64,
}

#[derive(Debug, Deserialize)]
pub struct StockReceiptItem {
    pub product_id:     i64,
    pub batch_number:   String,
    pub expiry_date:    String,
    pub quantity:       i64,
    pub purchase_price: f64,
    pub mrp:            f64,
    pub supplier_id:    Option<i64>,
}

#[derive(Debug, Serialize)]
pub struct LowStockAlert {
    pub id:            i64,
    pub sku:           String,
    pub name:          String,
    pub reorder_level: i64,
    pub total_stock:   i64,
}

// ─────────────────────────────────────────────────────────────────────────────
// Commands
// ─────────────────────────────────────────────────────────────────────────────

/// Full-text search on product name, SKU, or generic name.
/// Returns up to 20 results ordered by available stock descending.
#[tauri::command]
pub async fn search_products(
    query: String,
    state: State<'_, AppState>,
) -> Result<Vec<ProductSearchResult>, String> {
    let db_arc = state.db.clone();

    // Offload blocking SQLite call to the blocking thread pool.
    // This keeps the Tokio async runtime free for other tasks.
    tokio::task::spawn_blocking(move || {
        let conn = db_arc.lock().map_err(|e| e.to_string())?;
        let pattern = format!("%{}%", query);

        let mut stmt = conn
            .prepare_cached(
                "SELECT p.id, p.sku, p.name, p.generic_name, p.manufacturer,
                        p.hsn_code, p.gst_rate, p.unit, p.mrp, p.is_scheduled,
                        p.reorder_level,
                        COALESCE(SUM(sb.quantity),0) AS total_qty
                   FROM products p
                   LEFT JOIN stock_batches sb
                          ON sb.product_id = p.id AND sb.is_deleted = 0
                         AND sb.expiry_date > strftime('%Y-%m','now')
                  WHERE p.is_deleted = 0
                    AND (p.name LIKE ?1 OR p.sku LIKE ?1 OR p.generic_name LIKE ?1)
                  GROUP BY p.id
                  ORDER BY total_qty DESC
                  LIMIT 20;",
            )
            .map_err(|e| e.to_string())?;

        let results = stmt
            .query_map(rusqlite::params![pattern], |r| {
                Ok(ProductSearchResult {
                    id:            r.get(0)?,
                    sku:           r.get(1)?,
                    name:          r.get(2)?,
                    generic_name:  r.get(3)?,
                    manufacturer:  r.get(4)?,
                    hsn_code:      r.get(5)?,
                    gst_rate:      r.get(6)?,
                    unit:          r.get(7)?,
                    mrp:           r.get(8)?,
                    is_scheduled:  r.get(9)?,
                    reorder_level: r.get(10)?,
                    total_qty:     r.get(11)?,
                })
            })
            .map_err(|e| e.to_string())?
            .collect::<Result<Vec<_>, _>>()
            .map_err(|e| e.to_string())?;

        Ok(results)
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Return all non-expired, in-stock batches for a product, ordered FEFO.
#[tauri::command]
pub async fn get_stock_batches(
    product_id: i64,
    state: State<'_, AppState>,
) -> Result<Vec<StockBatch>, String> {
    let db_arc = state.db.clone();

    tokio::task::spawn_blocking(move || {
        let conn = db_arc.lock().map_err(|e| e.to_string())?;
        let mut stmt = conn
            .prepare_cached(
                "SELECT id, batch_number, expiry_date, quantity, mrp, purchase_price
                   FROM stock_batches
                  WHERE product_id = ?1 AND is_deleted = 0 AND quantity > 0
                    AND expiry_date > strftime('%Y-%m','now')
                  ORDER BY expiry_date ASC;",
            )
            .map_err(|e| e.to_string())?;

        let batches = stmt
            .query_map(rusqlite::params![product_id], |r| {
                Ok(StockBatch {
                    id:             r.get(0)?,
                    batch_number:   r.get(1)?,
                    expiry_date:    r.get(2)?,
                    quantity:       r.get(3)?,
                    mrp:            r.get(4)?,
                    purchase_price: r.get(5)?,
                })
            })
            .map_err(|e| e.to_string())?
            .collect::<Result<Vec<_>, _>>()
            .map_err(|e| e.to_string())?;

        Ok(batches)
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Record a goods receipt (new stock arriving from a supplier).
#[tauri::command]
pub async fn receive_stock(
    items: Vec<StockReceiptItem>,
    state: State<'_, AppState>,
) -> Result<usize, String> {
    let db_arc    = state.db.clone();
    let device_id = state.device_id.clone();
    let sync_tx   = state.sync_tx.clone();

    tokio::task::spawn_blocking(move || {
        let conn = db_arc.lock().map_err(|e| e.to_string())?;
        let ts   = now_us();
        let mut inserted = 0usize;

        for item in &items {
            conn.execute(
                "INSERT INTO stock_batches
                    (product_id, batch_number, expiry_date, quantity,
                     purchase_price, mrp, supplier_id, updated_at_us, device_id)
                 VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)
                 ON CONFLICT(product_id, batch_number) DO UPDATE SET
                     quantity       = quantity + excluded.quantity,
                     purchase_price = excluded.purchase_price,
                     mrp            = excluded.mrp,
                     updated_at_us  = excluded.updated_at_us,
                     row_version    = row_version + 1,
                     device_id      = excluded.device_id;",
                rusqlite::params![
                    item.product_id,
                    item.batch_number,
                    item.expiry_date,
                    item.quantity,
                    item.purchase_price,
                    item.mrp,
                    item.supplier_id,
                    ts,
                    device_id,
                ],
            )
            .map_err(|e| e.to_string())?;
            inserted += 1;
        }

        // Kick the sync agent to ship the new outbox entries
        let _ = sync_tx.send(crate::state::SyncTrigger);
        Ok(inserted)
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Return all products currently below their reorder level.
#[tauri::command]
pub async fn get_low_stock_alerts(
    state: State<'_, AppState>,
) -> Result<Vec<LowStockAlert>, String> {
    let db_arc = state.db.clone();

    tokio::task::spawn_blocking(move || {
        let conn = db_arc.lock().map_err(|e| e.to_string())?;
        let mut stmt = conn
            .prepare_cached(
                "SELECT id, sku, name, reorder_level,
                        COALESCE(SUM(sb.quantity),0) AS total_stock
                   FROM products p
                   LEFT JOIN stock_batches sb
                          ON sb.product_id = p.id AND sb.is_deleted = 0
                         AND sb.expiry_date > strftime('%Y-%m','now')
                  WHERE p.is_deleted = 0
                  GROUP BY p.id
                 HAVING total_stock <= p.reorder_level
                  ORDER BY total_stock ASC
                  LIMIT 100;",
            )
            .map_err(|e| e.to_string())?;

        let alerts = stmt
            .query_map(rusqlite::params![], |r| {
                Ok(LowStockAlert {
                    id:            r.get(0)?,
                    sku:           r.get(1)?,
                    name:          r.get(2)?,
                    reorder_level: r.get(3)?,
                    total_stock:   r.get(4)?,
                })
            })
            .map_err(|e| e.to_string())?
            .collect::<Result<Vec<_>, _>>()
            .map_err(|e| e.to_string())?;

        Ok(alerts)
    })
    .await
    .map_err(|e| e.to_string())?
}
