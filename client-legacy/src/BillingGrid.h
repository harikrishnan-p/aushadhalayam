#pragma once
// =============================================================================
// BillingGrid.h  —  Virtual wxGridTableBase model for the live billing ledger
// =============================================================================

#include <wx/grid.h>
#include <wx/colour.h>
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// BatchOption  —  one available stock batch for a product
// ─────────────────────────────────────────────────────────────────────────────
struct BatchOption {
    int         id;
    std::string batch_number;
    std::string expiry_date;  // YYYY-MM
    double      mrp;
    int         quantity;

    // Display label shown in the choice editor dropdown
    std::string label() const {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s (%s) qty:%d",
            batch_number.c_str(), expiry_date.c_str(), quantity);
        return buf;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BillItemRow  —  one line on the active bill
// ─────────────────────────────────────────────────────────────────────────────
struct BillItemRow {
    int         product_id;
    int         batch_id;
    std::string product_name;
    std::string hsn_code;
    std::string batch_number;
    std::string expiry_date;
    int         quantity;
    double      mrp;
    double      discount_pct;
    double      unit_price;
    double      gst_rate;
    double      taxable_value;
    double      cgst_amount;
    double      sgst_amount;
    double      igst_amount;
    double      line_total;
    int         is_scheduled;  // 0=OTC 1=H 2=H1 3=X

    std::vector<BatchOption> batch_options;  // populated after batch lookup
};

// ─────────────────────────────────────────────────────────────────────────────
// Column enumeration
// ─────────────────────────────────────────────────────────────────────────────
enum BillCol {
    BC_SERIAL     = 0,
    BC_MEDICINE   = 1,   // name + [SCH] prefix
    BC_BATCH      = 2,   // choice editor from batch_options
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
// BillingTableModel
// ─────────────────────────────────────────────────────────────────────────────
class BillingTableModel : public wxGridTableBase {
public:
    BillingTableModel();

    int      GetNumberRows() override;
    int      GetNumberCols() override;
    wxString GetValue(int row, int col) override;
    void     SetValue(int row, int col, const wxString& value) override;
    wxString GetColLabelValue(int col) override;
    bool     IsEmptyCell(int row, int col) override;

    wxGridCellAttr* GetAttr(int row, int col,
                            wxGridCellAttr::wxAttrKind kind) override;

    void AppendItem   (const BillItemRow& item);
    bool UpdateQty    (int row, int qty);
    bool UpdateDiscount(int row, double disc_pct);
    void RemoveItem   (int row);
    void Clear        ();

    const BillItemRow& GetItem(int row) const { return m_items[row]; }
    size_t             Count()           const { return m_items.size(); }

    // Returns true if any row has is_scheduled > 0
    bool HasScheduledItem() const;

private:
    std::vector<BillItemRow> m_items;

    void RecomputeLine(BillItemRow& item) const;
    bool IsNearExpiry (const std::string& expiry_ym) const;
    bool IsExpired    (const std::string& expiry_ym) const;
};
