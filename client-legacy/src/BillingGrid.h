#pragma once
// =============================================================================
// BillingGrid.h  —  Virtual wxGridTableBase model for the live billing ledger
//
// Uses wxGridTableBase (data-model pattern) rather than populating cell values
// directly.  wxGrid calls GetValue() only for visible cells, so the full item
// list can contain thousands of rows without any performance penalty.
//
// Thread safety: all public methods must be called from the GUI thread only.
// The worker thread communicates new items via wxCommandEvent (see MainFrame).
// =============================================================================

#include <wx/grid.h>
#include <wx/colour.h>
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// BillItemRow  —  one line on the active bill
// ─────────────────────────────────────────────────────────────────────────────
struct BillItemRow {
    int         product_id;
    int         batch_id;
    std::string product_name;
    std::string hsn_code;
    std::string batch_number;
    std::string expiry_date;    // YYYY-MM
    int         quantity;
    double      mrp;
    double      discount_pct;
    double      unit_price;     // mrp × (1 − disc/100)
    double      gst_rate;
    double      taxable_value;
    double      cgst_amount;
    double      sgst_amount;
    double      igst_amount;
    double      line_total;
};

// ─────────────────────────────────────────────────────────────────────────────
// Column enumeration (keeps column indices in one place)
// ─────────────────────────────────────────────────────────────────────────────
enum BillCol {
    BC_SERIAL     = 0,
    BC_MEDICINE   = 1,
    BC_BATCH      = 2,
    BC_EXPIRY     = 3,
    BC_QTY        = 4,
    BC_MRP        = 5,
    BC_DISC       = 6,
    BC_TAXABLE    = 7,
    BC_GST_RATE   = 8,
    BC_LINE_TOTAL = 9,
    BC_COUNT      = 10,
};

// ─────────────────────────────────────────────────────────────────────────────
// BillingTableModel  —  the wxGridTableBase implementation
// ─────────────────────────────────────────────────────────────────────────────
class BillingTableModel : public wxGridTableBase {
public:
    BillingTableModel();

    // ── wxGridTableBase overrides ─────────────────────────────────────────────
    int      GetNumberRows() override;
    int      GetNumberCols() override;
    wxString GetValue(int row, int col) override;
    void     SetValue(int row, int col, const wxString& value) override;
    wxString GetColLabelValue(int col) override;
    bool     IsEmptyCell(int row, int col) override;

    // Per-cell visual attributes (expired = red bg; near-expiry = yellow bg)
    wxGridCellAttr* GetAttr(int row, int col,
                            wxGridCellAttr::wxAttrKind kind) override;

    // ── Data management (GUI thread only) ────────────────────────────────────

    // Append a new line item; notifies the grid to add a row
    void AppendItem(const BillItemRow& item);

    // Update quantity / discount of an existing row and recompute GST
    bool UpdateQty     (int row, int qty);
    bool UpdateDiscount(int row, double disc_pct);

    // Remove one row; notifies the grid
    void RemoveItem(int row);

    // Clear all rows; notifies the grid
    void Clear();

    // Read-only access for the MainFrame to compute totals
    const BillItemRow& GetItem(int row) const { return m_items[row]; }
    size_t             Count()           const { return m_items.size(); }

private:
    std::vector<BillItemRow> m_items;

    void RecomputeLine(BillItemRow& item) const;
    bool IsNearExpiry (const std::string& expiry_ym) const; // within 3 months
    bool IsExpired    (const std::string& expiry_ym) const;
};
