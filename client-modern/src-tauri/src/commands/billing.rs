// =============================================================================
// commands/billing.rs  —  POS checkout and bill management Tauri commands
// =============================================================================

use crate::db::now_us;
use crate::state::{AppState, SyncTrigger};
use rusqlite::TransactionBehavior;
use serde::{Deserialize, Serialize};
use tauri::State;

// ─────────────────────────────────────────────────────────────────────────────
// DTOs
// ─────────────────────────────────────────────────────────────────────────────

/// One line item submitted from the POS frontend.
#[derive(Debug, Deserialize, Clone)]
pub struct CartItem {
    pub product_id:   i64,
    pub batch_id:     i64,
    pub product_name: String,
    pub hsn_code:     String,
    pub batch_number: String,
    pub expiry_date:  String,
    pub quantity:     i64,
    pub mrp:          f64,
    pub discount_pct: f64,
    pub gst_rate:     f64,
}

/// Full checkout payload from the frontend.
#[derive(Debug, Deserialize)]
pub struct CheckoutRequest {
    pub customer_name:     Option<String>,
    pub customer_id:       Option<i64>,
    pub payment_mode:      String,
    pub bill_discount_pct: f64,
    pub is_interstate:     bool,
    pub items:             Vec<CartItem>,
}

/// Computed line amounts (sent back to the frontend for display and print).
#[derive(Debug, Serialize, Clone)]
pub struct ComputedLineItem {
    pub product_id:    i64,
    pub batch_id:      i64,
    pub product_name:  String,
    pub hsn_code:      String,
    pub batch_number:  String,
    pub expiry_date:   String,
    pub quantity:      i64,
    pub mrp:           f64,
    pub discount_pct:  f64,
    pub unit_price:    f64,
    pub gst_rate:      f64,
    pub taxable_value: f64,
    pub cgst_amount:   f64,
    pub sgst_amount:   f64,
    pub igst_amount:   f64,
    pub line_total:    f64,
}

/// Result returned to the frontend after a successful checkout.
#[derive(Debug, Serialize)]
pub struct BillResult {
    pub bill_id:         i64,
    pub bill_number:     String,
    pub bill_date:       String,
    pub customer_name:   Option<String>,
    pub taxable_amount:  f64,
    pub cgst_amount:     f64,
    pub sgst_amount:     f64,
    pub igst_amount:     f64,
    pub total_gst:       f64,
    pub discount_amount: f64,
    pub grand_total:     f64,
    pub payment_mode:    String,
    pub items:           Vec<ComputedLineItem>,
}

// ─────────────────────────────────────────────────────────────────────────────
// GST computation (mirrors GstEngine.h logic exactly)
// ─────────────────────────────────────────────────────────────────────────────

fn round2(v: f64) -> f64 {
    (v * 100.0 + 0.5).floor() / 100.0
}

pub fn compute_line(item: &CartItem, is_interstate: bool) -> ComputedLineItem {
    let unit_price    = round2(item.mrp * (1.0 - item.discount_pct / 100.0));
    let line_total    = round2(unit_price * item.quantity as f64);
    let taxable_value = round2(line_total / (1.0 + item.gst_rate / 100.0));
    let gst           = round2(line_total - taxable_value);

    let (cgst, sgst, igst) = if is_interstate {
        (0.0, 0.0, gst)
    } else {
        let c = round2(gst / 2.0);
        (c, round2(gst - c), 0.0)
    };

    ComputedLineItem {
        product_id:    item.product_id,
        batch_id:      item.batch_id,
        product_name:  item.product_name.clone(),
        hsn_code:      item.hsn_code.clone(),
        batch_number:  item.batch_number.clone(),
        expiry_date:   item.expiry_date.clone(),
        quantity:      item.quantity,
        mrp:           item.mrp,
        discount_pct:  item.discount_pct,
        unit_price,
        gst_rate:      item.gst_rate,
        taxable_value,
        cgst_amount:   cgst,
        sgst_amount:   sgst,
        igst_amount:   igst,
        line_total,
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Commands
// ─────────────────────────────────────────────────────────────────────────────

/// Atomic POS checkout: validates stock, deducts inventory, inserts bill + items.
///
/// Uses rusqlite `Transaction` (BEGIN IMMEDIATE) so the `Transaction` RAII guard
/// automatically rolls back on any early return — no manual ROLLBACK calls needed.
#[tauri::command]
pub async fn process_checkout(
    req:   CheckoutRequest,
    state: State<'_, AppState>,
) -> Result<BillResult, String> {
    if req.items.is_empty() {
        return Err("Cart is empty".to_string());
    }

    let db_arc    = state.db.clone();
    let device_id = state.device_id.clone();
    let sync_tx   = state.sync_tx.clone();

    tokio::task::spawn_blocking(move || {
        let mut conn = db_arc.lock().map_err(|e| e.to_string())?;

        // ── Compute all line amounts before touching the DB ──────────────────
        let lines: Vec<ComputedLineItem> = req.items
            .iter()
            .map(|it| compute_line(it, req.is_interstate))
            .collect();

        let sum_taxable: f64 = lines.iter().map(|l| l.taxable_value).sum();
        let sum_cgst:    f64 = lines.iter().map(|l| l.cgst_amount).sum();
        let sum_sgst:    f64 = lines.iter().map(|l| l.sgst_amount).sum();
        let sum_igst:    f64 = lines.iter().map(|l| l.igst_amount).sum();
        let sum_total:   f64 = lines.iter().map(|l| l.line_total).sum();

        let discount_amount = round2(sum_total * req.bill_discount_pct / 100.0);
        let grand_total     = round2(sum_total - discount_amount);
        let total_gst       = round2(sum_cgst + sum_sgst + sum_igst);

        // ── BEGIN IMMEDIATE via RAII Transaction ─────────────────────────────
        // On any early return (? or explicit Err), Transaction::drop() rolls back.
        let tx = conn
            .transaction_with_behavior(TransactionBehavior::Immediate)
            .map_err(|e| e.to_string())?;

        // Verify stock levels (inside the locked transaction to prevent TOCTOU)
        for (item, _) in req.items.iter().zip(lines.iter()) {
            let avail: i64 = tx
                .query_row(
                    "SELECT quantity FROM stock_batches
                      WHERE id = ?1 AND is_deleted = 0;",
                    rusqlite::params![item.batch_id],
                    |r| r.get(0),
                )
                .map_err(|_| format!("Batch '{}' not found", item.batch_number))?;

            if avail < item.quantity {
                return Err(format!(
                    "Insufficient stock for '{}': have {avail}, need {}",
                    item.product_name, item.quantity
                ));
            }
        }

        let ts = now_us();

        // Deduct stock (triggers write sync_outbox entries for stock_batches)
        for item in &req.items {
            tx.execute(
                "UPDATE stock_batches
                    SET quantity      = quantity - ?1,
                        updated_at_us = ?2,
                        row_version   = row_version + 1,
                        device_id     = ?3
                  WHERE id = ?4;",
                rusqlite::params![item.quantity, ts, device_id, item.batch_id],
            )
            .map_err(|e| e.to_string())?;
        }

        // ── Generate bill number ─────────────────────────────────────────────
        let today  = chrono::Local::now().format("%Y%m%d").to_string();
        let serial: i64 = tx
            .query_row(
                "SELECT COUNT(*)+1 FROM bills WHERE bill_date LIKE ?1||'%';",
                rusqlite::params![chrono::Local::now().format("%Y-%m-%d").to_string()],
                |r| r.get(0),
            )
            .unwrap_or(1);
        let bill_number = format!("BILL-{}-{:04}", today, serial);
        let bill_date   = chrono::Local::now().format("%Y-%m-%dT%H:%M:%S").to_string();

        // ── Insert bill header (trigger writes bills outbox entry) ───────────
        tx.execute(
            "INSERT INTO bills
                (bill_number, bill_date, customer_id, customer_name,
                 total_amount, discount_amount, taxable_amount,
                 cgst_amount, sgst_amount, igst_amount, total_gst_amount,
                 grand_total, payment_mode, updated_at_us, device_id)
             VALUES
                (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15);",
            rusqlite::params![
                bill_number,
                bill_date,
                req.customer_id,
                req.customer_name,
                round2(sum_total),
                discount_amount,
                round2(sum_taxable),
                round2(sum_cgst),
                round2(sum_sgst),
                round2(sum_igst),
                total_gst,
                grand_total,
                req.payment_mode,
                ts,
                device_id,
            ],
        )
        .map_err(|e| e.to_string())?;

        let bill_id = tx.last_insert_rowid();

        // ── Insert bill items (triggers write bill_items outbox entries) ──────
        for line in &lines {
            tx.execute(
                "INSERT INTO bill_items
                    (bill_id, product_id, batch_id, product_name, hsn_code,
                     batch_number, expiry_date, quantity, mrp, discount_pct,
                     unit_price, gst_rate, taxable_value,
                     cgst_amount, sgst_amount, igst_amount, line_total,
                     updated_at_us, device_id)
                 VALUES
                    (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19);",
                rusqlite::params![
                    bill_id,
                    line.product_id,
                    line.batch_id,
                    line.product_name,
                    line.hsn_code,
                    line.batch_number,
                    line.expiry_date,
                    line.quantity,
                    line.mrp,
                    line.discount_pct,
                    line.unit_price,
                    line.gst_rate,
                    line.taxable_value,
                    line.cgst_amount,
                    line.sgst_amount,
                    line.igst_amount,
                    line.line_total,
                    ts,
                    device_id,
                ],
            )
            .map_err(|e| e.to_string())?;
        }

        // ── COMMIT — sync_outbox triggers fire here atomically ───────────────
        tx.commit().map_err(|e| e.to_string())?;

        let _ = sync_tx.send(SyncTrigger);

        Ok(BillResult {
            bill_id,
            bill_number,
            bill_date,
            customer_name:  req.customer_name,
            taxable_amount: round2(sum_taxable),
            cgst_amount:    round2(sum_cgst),
            sgst_amount:    round2(sum_sgst),
            igst_amount:    round2(sum_igst),
            total_gst,
            discount_amount,
            grand_total,
            payment_mode:   req.payment_mode,
            items:          lines,
        })
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Cancel a previously issued bill and restore stock inventory.
#[tauri::command]
pub async fn cancel_bill(
    bill_id: i64,
    reason:  String,
    state:   State<'_, AppState>,
) -> Result<(), String> {
    let db_arc    = state.db.clone();
    let device_id = state.device_id.clone();
    let sync_tx   = state.sync_tx.clone();

    tokio::task::spawn_blocking(move || {
        let mut conn = db_arc.lock().map_err(|e| e.to_string())?;
        let ts       = now_us();

        let tx = conn
            .transaction_with_behavior(TransactionBehavior::Immediate)
            .map_err(|e| e.to_string())?;

        // Fetch items to restore (collect first — can't hold stmt while executing)
        let restores: Vec<(i64, i64)> = {
            let mut stmt = tx
                .prepare(
                    "SELECT batch_id, quantity FROM bill_items
                      WHERE bill_id = ?1 AND is_deleted = 0;",
                )
                .map_err(|e| e.to_string())?;
            let rows: Vec<(i64, i64)> = stmt
                .query_map(rusqlite::params![bill_id], |r| Ok((r.get(0)?, r.get(1)?)))
                .map_err(|e| e.to_string())?
                .collect::<Result<_, rusqlite::Error>>()
                .map_err(|e| e.to_string())?;
            rows
        };

        for (batch_id, qty) in restores {
            tx.execute(
                "UPDATE stock_batches
                    SET quantity      = quantity + ?1,
                        updated_at_us = ?2,
                        row_version   = row_version + 1,
                        device_id     = ?3
                  WHERE id = ?4;",
                rusqlite::params![qty, ts, device_id, batch_id],
            )
            .map_err(|e| e.to_string())?;
        }

        tx.execute(
            "UPDATE bills
                SET is_cancelled  = 1,
                    cancel_reason = ?1,
                    updated_at_us = ?2,
                    row_version   = row_version + 1,
                    device_id     = ?3
              WHERE id = ?4;",
            rusqlite::params![reason, ts, device_id, bill_id],
        )
        .map_err(|e| e.to_string())?;

        tx.commit().map_err(|e| e.to_string())?;
        let _ = sync_tx.send(SyncTrigger);
        Ok(())
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Return sync queue depth (displayed in the status bar).
#[tauri::command]
pub async fn get_pending_sync_count(
    state: State<'_, AppState>,
) -> Result<i64, String> {
    let db_arc = state.db.clone();
    tokio::task::spawn_blocking(move || {
        let conn = db_arc.lock().map_err(|e| e.to_string())?;
        let count: i64 = conn
            .query_row(
                "SELECT COUNT(*) FROM sync_outbox WHERE is_transmitted=0;",
                [],
                |r| r.get(0),
            )
            .map_err(|e| e.to_string())?;
        Ok(count)
    })
    .await
    .map_err(|e| e.to_string())?
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests
// ─────────────────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::{compute_line, CartItem};

    fn item(mrp: f64, qty: i64, disc: f64, rate: f64) -> CartItem {
        CartItem {
            product_id: 1, batch_id: 1,
            product_name: "Test".into(), hsn_code: "3004".into(),
            batch_number: "B1".into(), expiry_date: "2027-12".into(),
            quantity: qty, mrp, discount_pct: disc, gst_rate: rate,
        }
    }

    #[test]
    fn gst_12_percent_intra_state() {
        let line = compute_line(&item(112.0, 1, 0.0, 12.0), false);
        assert!((line.taxable_value - 100.0).abs() < 0.01, "taxable={}", line.taxable_value);
        assert!((line.cgst_amount   -   6.0).abs() < 0.01, "cgst={}", line.cgst_amount);
        assert!((line.sgst_amount   -   6.0).abs() < 0.01, "sgst={}", line.sgst_amount);
        assert!((line.line_total    - 112.0).abs() < 0.01, "total={}", line.line_total);
    }

    #[test]
    fn gst_18_percent_interstate() {
        let line = compute_line(&item(118.0, 2, 0.0, 18.0), true);
        // MRP 118 @18% → taxable = 118/1.18 = 100.00 per unit, ×2 = 200.00
        assert!((line.taxable_value - 200.0).abs() < 0.01, "taxable={}", line.taxable_value);
        assert!((line.igst_amount   -  36.0).abs() < 0.01, "igst={}", line.igst_amount);
        assert_eq!(line.cgst_amount, 0.0);
        assert_eq!(line.sgst_amount, 0.0);
    }

    #[test]
    fn discount_applied_before_gst() {
        // MRP 100, 10% discount → effective MRP 90, @5%
        // taxable = 90 / 1.05 = 85.71
        let line = compute_line(&item(100.0, 1, 10.0, 5.0), false);
        assert!((line.unit_price    -  90.0).abs() < 0.01, "unit={}", line.unit_price);
        assert!((line.taxable_value -  85.71).abs() < 0.01, "taxable={}", line.taxable_value);
    }
}
