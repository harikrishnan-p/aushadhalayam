// =============================================================================
// BillingGrid.cpp
// =============================================================================

#include "BillingGrid.h"
#include "GstEngine.h"

#include <wx/colour.h>
#include <ctime>
#include <cstdio>
#include <cstdlib>

BillingTableModel::BillingTableModel() {}

// ─────────────────────────────────────────────────────────────────────────────
// wxGridTableBase required overrides
// ─────────────────────────────────────────────────────────────────────────────

int BillingTableModel::GetNumberRows() {
    return static_cast<int>(m_items.size());
}

int BillingTableModel::GetNumberCols() {
    return BC_COUNT;
}

bool BillingTableModel::IsEmptyCell(int /*row*/, int /*col*/) {
    return false;
}

wxString BillingTableModel::GetColLabelValue(int col) {
    switch (col) {
        case BC_SERIAL:     return "#";
        case BC_MEDICINE:   return "Medicine";
        case BC_BATCH:      return "Batch";
        case BC_EXPIRY:     return "Expiry";
        case BC_QTY:        return "Qty";
        case BC_MRP:        return "MRP";
        case BC_DISC:       return "Disc %";
        case BC_TAXABLE:    return "Taxable";
        case BC_GST_RATE:   return "GST%";
        case BC_LINE_TOTAL: return "Total";
        default:            return "";
    }
}

wxString BillingTableModel::GetValue(int row, int col) {
    if (row < 0 || row >= static_cast<int>(m_items.size())) return "";
    const BillItemRow& it = m_items[row];
    char buf[64];
    switch (col) {
        case BC_SERIAL:
            std::snprintf(buf, sizeof(buf), "%d", row + 1);
            return buf;
        case BC_MEDICINE:
            return wxString::FromUTF8(it.product_name.c_str());
        case BC_BATCH:
            return wxString::FromUTF8(it.batch_number.c_str());
        case BC_EXPIRY:
            // Convert YYYY-MM to MM/YY for compact display
            if (it.expiry_date.size() >= 7) {
                std::snprintf(buf, sizeof(buf), "%s/%s",
                    it.expiry_date.substr(5,2).c_str(),
                    it.expiry_date.substr(2,2).c_str());
                return buf;
            }
            return wxString::FromUTF8(it.expiry_date.c_str());
        case BC_QTY:
            std::snprintf(buf, sizeof(buf), "%d", it.quantity);
            return buf;
        case BC_MRP:
            std::snprintf(buf, sizeof(buf), "%.2f", it.mrp);
            return buf;
        case BC_DISC:
            std::snprintf(buf, sizeof(buf), "%.1f", it.discount_pct);
            return buf;
        case BC_TAXABLE:
            std::snprintf(buf, sizeof(buf), "%.2f", it.taxable_value);
            return buf;
        case BC_GST_RATE:
            std::snprintf(buf, sizeof(buf), "%.1f%%", it.gst_rate);
            return buf;
        case BC_LINE_TOTAL:
            std::snprintf(buf, sizeof(buf), "%.2f", it.line_total);
            return buf;
        default:
            return "";
    }
}

void BillingTableModel::SetValue(int row, int col, const wxString& value) {
    if (row < 0 || row >= static_cast<int>(m_items.size())) return;
    BillItemRow& it = m_items[row];
    std::string v = value.ToStdString();

    if (col == BC_QTY) {
        int qty = std::atoi(v.c_str());
        if (qty > 0) {
            it.quantity = qty;
            RecomputeLine(it);
        }
    } else if (col == BC_DISC) {
        double d = std::atof(v.c_str());
        if (d >= 0.0 && d <= 100.0) {
            it.discount_pct = d;
            RecomputeLine(it);
        }
    }
    // Notify grid that this cell changed (triggers repaint)
    if (GetView()) {
        wxGridTableMessage msg(this, wxGRIDTABLE_REQUEST_VIEW_GET_VALUES, row, 1);
        GetView()->ProcessTableMessage(msg);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Visual attributes — expired = red background; near-expiry = amber
// ─────────────────────────────────────────────────────────────────────────────

wxGridCellAttr* BillingTableModel::GetAttr(int row, int col,
                                           wxGridCellAttr::wxAttrKind kind) {
    if (row < 0 || row >= static_cast<int>(m_items.size()))
        return wxGridTableBase::GetAttr(row, col, kind);

    const BillItemRow& it = m_items[row];

    wxGridCellAttr* attr = nullptr;

    if (IsExpired(it.expiry_date)) {
        attr = new wxGridCellAttr();
        attr->SetBackgroundColour(wxColour(255, 180, 180));   // soft red
        attr->SetTextColour(*wxBLACK);
    } else if (IsNearExpiry(it.expiry_date)) {
        attr = new wxGridCellAttr();
        attr->SetBackgroundColour(wxColour(255, 243, 176));   // amber
        attr->SetTextColour(*wxBLACK);
    }

    // Editable columns: Qty and Disc
    bool editable = (col == BC_QTY || col == BC_DISC);
    if (!editable) {
        if (!attr) attr = new wxGridCellAttr();
        attr->SetReadOnly(true);
    }

    if (attr) return attr;
    return wxGridTableBase::GetAttr(row, col, kind);
}

// ─────────────────────────────────────────────────────────────────────────────
// Data management
// ─────────────────────────────────────────────────────────────────────────────

void BillingTableModel::AppendItem(const BillItemRow& item) {
    m_items.push_back(item);

    if (GetView()) {
        wxGridTableMessage msg(this,
            wxGRIDTABLE_NOTIFY_ROWS_APPENDED, 1);
        GetView()->ProcessTableMessage(msg);
    }
}

bool BillingTableModel::UpdateQty(int row, int qty) {
    if (row < 0 || row >= static_cast<int>(m_items.size())) return false;
    if (qty <= 0) return false;
    m_items[row].quantity = qty;
    RecomputeLine(m_items[row]);
    if (GetView()) GetView()->RefreshRect(
        GetView()->CellToRect(row, BC_QTY), false);
    return true;
}

bool BillingTableModel::UpdateDiscount(int row, double disc_pct) {
    if (row < 0 || row >= static_cast<int>(m_items.size())) return false;
    if (disc_pct < 0.0 || disc_pct > 100.0) return false;
    m_items[row].discount_pct = disc_pct;
    RecomputeLine(m_items[row]);
    if (GetView()) GetView()->RefreshRect(
        GetView()->CellToRect(row, BC_DISC), false);
    return true;
}

void BillingTableModel::RemoveItem(int row) {
    if (row < 0 || row >= static_cast<int>(m_items.size())) return;
    m_items.erase(m_items.begin() + row);

    if (GetView()) {
        wxGridTableMessage msg(this,
            wxGRIDTABLE_NOTIFY_ROWS_DELETED, row, 1);
        GetView()->ProcessTableMessage(msg);
    }
}

void BillingTableModel::Clear() {
    int n = static_cast<int>(m_items.size());
    m_items.clear();

    if (GetView() && n > 0) {
        wxGridTableMessage msg(this,
            wxGRIDTABLE_NOTIFY_ROWS_DELETED, 0, n);
        GetView()->ProcessTableMessage(msg);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void BillingTableModel::RecomputeLine(BillItemRow& it) const {
    // Assume intra-state (CGST+SGST); caller sets is_interstate per session
    gst::LineResult r = gst::compute_line(
        it.mrp, it.quantity, it.discount_pct, it.gst_rate, /*interstate=*/false);
    it.unit_price    = r.unit_price;
    it.taxable_value = r.taxable_value;
    it.cgst_amount   = r.cgst_amount;
    it.sgst_amount   = r.sgst_amount;
    it.igst_amount   = r.igst_amount;
    it.line_total    = r.line_total;
}

static std::string current_ym() {
    time_t t = time(nullptr);
    struct tm* lt = localtime(&t);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04d-%02d", lt->tm_year + 1900, lt->tm_mon + 1);
    return buf;
}

bool BillingTableModel::IsExpired(const std::string& expiry_ym) const {
    return expiry_ym < current_ym();
}

bool BillingTableModel::IsNearExpiry(const std::string& expiry_ym) const {
    // Near-expiry: expires within 3 months
    time_t t = time(nullptr);
    struct tm* lt = localtime(&t);
    // Advance 3 months
    lt->tm_mon += 3;
    if (lt->tm_mon >= 12) { lt->tm_year++; lt->tm_mon -= 12; }
    char threshold[8];
    std::snprintf(threshold, sizeof(threshold), "%04d-%02d",
                  lt->tm_year + 1900, lt->tm_mon + 1);
    return expiry_ym <= threshold && !IsExpired(expiry_ym);
}
