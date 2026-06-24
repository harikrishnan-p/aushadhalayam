// =============================================================================
// commands/gst.rs  —  GST reporting and GSTR-1 data export
// =============================================================================

use crate::state::AppState;
use serde::Serialize;
use tauri::State;

// ─────────────────────────────────────────────────────────────────────────────
// DTOs
// ─────────────────────────────────────────────────────────────────────────────

/// One row in the GSTR-1 B2C HSN-wise summary (Table 7 of GSTR-1 return)
#[derive(Debug, Serialize)]
pub struct Gstr1B2cRow {
    pub month:          String,   // MM
    pub year:           String,   // YYYY
    pub hsn_code:       String,
    pub gst_rate:       f64,
    pub taxable_value:  f64,
    pub total_cgst:     f64,
    pub total_sgst:     f64,
    pub total_igst:     f64,
    pub invoice_count:  i64,
}

/// Summary totals for a billing period
#[derive(Debug, Serialize)]
pub struct PeriodSummary {
    pub period:          String,
    pub total_invoices:  i64,
    pub total_taxable:   f64,
    pub total_cgst:      f64,
    pub total_sgst:      f64,
    pub total_igst:      f64,
    pub total_gst:       f64,
    pub grand_total:     f64,
}

/// A bill-level GST breakdown (for B2B invoice list)
#[derive(Debug, Serialize)]
pub struct BillGstRow {
    pub bill_id:        i64,
    pub bill_number:    String,
    pub bill_date:      String,
    pub hsn_code:       String,
    pub gst_rate:       f64,
    pub taxable_value:  f64,
    pub cgst:           f64,
    pub sgst:           f64,
    pub igst:           f64,
    pub line_total:     f64,
}

// ─────────────────────────────────────────────────────────────────────────────
// Commands
// ─────────────────────────────────────────────────────────────────────────────

/// Return GSTR-1 B2C HSN-wise summary for a given period (MMYYYY).
/// This data maps directly to Table 7 of the GSTR-1 return filed on GST portal.
#[tauri::command]
pub async fn get_gstr1_b2c_summary(
    period: String,   // "062026" → June 2026
    state:  State<'_, AppState>,
) -> Result<Vec<Gstr1B2cRow>, String> {
    // Validate period format
    if period.len() != 6 {
        return Err("Period must be in MMYYYY format (e.g., 062026)".to_string());
    }
    let mm   = &period[0..2];
    let yyyy = &period[2..6];

    let db_arc    = state.db.clone();
    let month_str = mm.to_string();
    let year_str  = yyyy.to_string();

    tokio::task::spawn_blocking(move || {
        let conn = db_arc.lock().map_err(|e| e.to_string())?;

        let mut stmt = conn
            .prepare_cached(
                "SELECT month, year, hsn_code, gst_rate,
                        taxable_value, total_cgst, total_sgst, total_igst,
                        invoice_count
                   FROM v_gstr1_b2c
                  WHERE month = ?1 AND year = ?2
                  ORDER BY hsn_code, gst_rate;",
            )
            .map_err(|e| e.to_string())?;

        let rows = stmt
            .query_map(rusqlite::params![month_str, year_str], |r| {
                Ok(Gstr1B2cRow {
                    month:         r.get(0)?,
                    year:          r.get(1)?,
                    hsn_code:      r.get(2)?,
                    gst_rate:      r.get(3)?,
                    taxable_value: r.get(4)?,
                    total_cgst:    r.get(5)?,
                    total_sgst:    r.get(6)?,
                    total_igst:    r.get(7)?,
                    invoice_count: r.get(8)?,
                })
            })
            .map_err(|e| e.to_string())?
            .collect::<Result<Vec<_>, _>>()
            .map_err(|e| e.to_string())?;

        Ok(rows)
    })
    .await
    .map_err(|e| e.to_string())?
}

/// Period summary for dashboard display (total sales, GST collected, etc.)
#[tauri::command]
pub async fn get_period_summary(
    period: String,
    state:  State<'_, AppState>,
) -> Result<PeriodSummary, String> {
    if period.len() != 6 {
        return Err("Period must be MMYYYY".to_string());
    }
    let mm   = period[0..2].to_string();
    let yyyy = period[2..6].to_string();
    let db_arc = state.db.clone();

    tokio::task::spawn_blocking(move || {
        let conn = db_arc.lock().map_err(|e| e.to_string())?;

        let row = conn
            .query_row(
                "SELECT
                    COUNT(DISTINCT b.id),
                    COALESCE(SUM(bi.taxable_value),0),
                    COALESCE(SUM(bi.cgst_amount),0),
                    COALESCE(SUM(bi.sgst_amount),0),
                    COALESCE(SUM(bi.igst_amount),0),
                    COALESCE(SUM(bi.line_total),0)
                   FROM bills b
                   JOIN bill_items bi ON bi.bill_id = b.id
                  WHERE b.is_cancelled = 0
                    AND bi.is_deleted  = 0
                    AND strftime('%m', b.bill_date) = ?1
                    AND strftime('%Y', b.bill_date) = ?2;",
                rusqlite::params![mm, yyyy],
                |r| {
                    let taxable: f64  = r.get(1)?;
                    let cgst:    f64  = r.get(2)?;
                    let sgst:    f64  = r.get(3)?;
                    let igst:    f64  = r.get(4)?;
                    let total:   f64  = r.get(5)?;
                    Ok(PeriodSummary {
                        period:         format!("{}{}", mm, yyyy),
                        total_invoices: r.get(0)?,
                        total_taxable:  taxable,
                        total_cgst:     cgst,
                        total_sgst:     sgst,
                        total_igst:     igst,
                        total_gst:      cgst + sgst + igst,
                        grand_total:    total,
                    })
                },
            )
            .map_err(|e| e.to_string())?;

        // Borrow-checker: mm/yyyy moved into closure above; reconstruct period
        Ok(row)
    })
    .await
    .map_err(|e| e.to_string())?
}

/// GSTR-2B Reconciliation: returns all line items for a period that can be
/// matched against purchase invoices uploaded to the GST portal.
#[tauri::command]
pub async fn get_gstr1_bill_details(
    period: String,
    state:  State<'_, AppState>,
) -> Result<Vec<BillGstRow>, String> {
    if period.len() != 6 {
        return Err("Period must be MMYYYY".to_string());
    }
    let mm   = period[0..2].to_string();
    let yyyy = period[2..6].to_string();
    let db_arc = state.db.clone();

    tokio::task::spawn_blocking(move || {
        let conn = db_arc.lock().map_err(|e| e.to_string())?;

        let mut stmt = conn
            .prepare_cached(
                "SELECT bill_id, bill_number, bill_date, hsn_code, gst_rate,
                        taxable_value, cgst, sgst, igst, line_total
                   FROM v_bill_gst_summary
                  WHERE strftime('%m', bill_date) = ?1
                    AND strftime('%Y', bill_date) = ?2
                  ORDER BY bill_date DESC, bill_id, hsn_code;",
            )
            .map_err(|e| e.to_string())?;

        let rows = stmt
            .query_map(rusqlite::params![mm, yyyy], |r| {
                Ok(BillGstRow {
                    bill_id:       r.get(0)?,
                    bill_number:   r.get(1)?,
                    bill_date:     r.get(2)?,
                    hsn_code:      r.get(3)?,
                    gst_rate:      r.get(4)?,
                    taxable_value: r.get(5)?,
                    cgst:          r.get(6)?,
                    sgst:          r.get(7)?,
                    igst:          r.get(8)?,
                    line_total:    r.get(9)?,
                })
            })
            .map_err(|e| e.to_string())?
            .collect::<Result<Vec<_>, _>>()
            .map_err(|e| e.to_string())?;

        Ok(rows)
    })
    .await
    .map_err(|e| e.to_string())?
}
