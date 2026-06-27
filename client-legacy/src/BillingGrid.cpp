// =============================================================================
// BillingGrid.cpp
// =============================================================================

#include "BillingGrid.h"
#include "GstEngine.h"
#include "AppColors.h"

#include <wx/colour.h>
#include <ctime>
#include <cstdio>
#include <cstdlib>

static const char* kSchedTag[] = { "", "[H]", "[H1]", "[X]" };

BillingTableModel::BillingTableModel() {}

int BillingTableModel::GetNumberRows() { return (int)m_items.size(); }
int BillingTableModel::GetNumberCols() { return BC_COUNT; }
bool BillingTableModel::IsEmptyCell(int, int) { return false; }

wxString BillingTableModel::GetColLabelValue(int col) {
    switch (col) {
        case BC_SERIAL:     return "#";
        case BC_MEDICINE:   return "Medicine";
        case BC_BATCH:      return "Batch";
        case BC_EXPIRY:     return "Expiry";
        case BC_QTY:        return "Qty";
        case BC_MRP:        return "MRP";
        case BC_DISC:       return "Disc%";
        case BC_TAXABLE:    return "Taxable";
        case BC_GST_RATE:   return "GST%";
        case BC_LINE_TOTAL: return "Total";
        default:            return "";
    }
}

wxString BillingTableModel::GetValue(int row, int col) {
    if (row < 0 || row >= (int)m_items.size()) return "";
    const BillItemRow& it = m_items[row];
    char buf[64];
    switch (col) {
        case BC_SERIAL:
            std::snprintf(buf, sizeof(buf), "%d", row + 1);
            return buf;
        case BC_MEDICINE: {
            std::string nm = it.product_name;
            if (it.is_scheduled > 0 && it.is_scheduled <= 3)
                nm = std::string(kSchedTag[it.is_scheduled]) + " " + nm;
            return wxString::FromUTF8(nm.c_str());
        }
        case BC_BATCH:
            // Return the label matching the selected batch_id, or the raw batch_number
            for (const auto& b : it.batch_options)
                if (b.id == it.batch_id) return wxString::FromUTF8(b.label().c_str());
            return wxString::FromUTF8(it.batch_number.c_str());
        case BC_EXPIRY:
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
        default: return "";
    }
}

void BillingTableModel::SetValue(int row, int col, const wxString& value) {
    if (row < 0 || row >= (int)m_items.size()) return;
    BillItemRow& it = m_items[row];
    std::string v = value.ToStdString();

    if (col == BC_QTY) {
        int qty = std::atoi(v.c_str());
        if (qty > 0) { it.quantity = qty; RecomputeLine(it); }
    } else if (col == BC_DISC) {
        double d = std::atof(v.c_str());
        if (d >= 0.0 && d <= 100.0) { it.discount_pct = d; RecomputeLine(it); }
    } else if (col == BC_BATCH) {
        // Find the selected batch option by label and update the row
        for (const auto& b : it.batch_options) {
            if (b.label() == v) {
                it.batch_id     = b.id;
                it.batch_number = b.batch_number;
                it.expiry_date  = b.expiry_date;
                it.mrp          = b.mrp;
                RecomputeLine(it);
                break;
            }
        }
    }

    if (GetView()) {
        wxGridTableMessage msg(this, wxGRIDTABLE_REQUEST_VIEW_GET_VALUES, row, 1);
        GetView()->ProcessTableMessage(msg);
    }
}

// ── Visual attributes ─────────────────────────────────────────────────────────

wxGridCellAttr* BillingTableModel::GetAttr(int row, int col,
                                           wxGridCellAttr::wxAttrKind kind) {
    if (row < 0 || row >= (int)m_items.size())
        return wxGridTableBase::GetAttr(row, col, kind);

    const BillItemRow& it = m_items[row];
    wxGridCellAttr* attr = nullptr;

    // Expiry colour
    if (IsExpired(it.expiry_date)) {
        attr = new wxGridCellAttr();
        attr->SetBackgroundColour(clr::DangerBg());
        attr->SetTextColour(clr::Text());
    } else if (IsNearExpiry(it.expiry_date)) {
        attr = new wxGridCellAttr();
        attr->SetBackgroundColour(clr::WarningBg());
        attr->SetTextColour(clr::Text());
    }

    // Batch column: choice editor if options available
    if (col == BC_BATCH && !it.batch_options.empty()) {
        if (!attr) attr = new wxGridCellAttr();
        wxArrayString choices;
        for (const auto& b : it.batch_options)
            choices.Add(wxString::FromUTF8(b.label().c_str()));
        attr->SetEditor(new wxGridCellChoiceEditor(choices));
        attr->SetReadOnly(false);
        return attr;
    }

    // Only Qty and Disc% are editable (not Batch — handled above)
    bool editable = (col == BC_QTY || col == BC_DISC);
    if (!editable) {
        if (!attr) attr = new wxGridCellAttr();
        attr->SetReadOnly(true);
    }

    if (attr) return attr;
    return wxGridTableBase::GetAttr(row, col, kind);
}

// ── Data management ───────────────────────────────────────────────────────────

void BillingTableModel::AppendItem(const BillItemRow& item) {
    m_items.push_back(item);
    RecomputeLine(m_items.back());
    if (GetView()) {
        wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, 1);
        GetView()->ProcessTableMessage(msg);
    }
}

bool BillingTableModel::UpdateQty(int row, int qty) {
    if (row < 0 || row >= (int)m_items.size() || qty <= 0) return false;
    m_items[row].quantity = qty;
    RecomputeLine(m_items[row]);
    return true;
}

bool BillingTableModel::UpdateDiscount(int row, double d) {
    if (row < 0 || row >= (int)m_items.size() || d < 0 || d > 100) return false;
    m_items[row].discount_pct = d;
    RecomputeLine(m_items[row]);
    return true;
}

void BillingTableModel::RemoveItem(int row) {
    if (row < 0 || row >= (int)m_items.size()) return;
    m_items.erase(m_items.begin() + row);
    if (GetView()) {
        wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, row, 1);
        GetView()->ProcessTableMessage(msg);
    }
}

void BillingTableModel::Clear() {
    int n = (int)m_items.size();
    m_items.clear();
    if (GetView() && n > 0) {
        wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, 0, n);
        GetView()->ProcessTableMessage(msg);
    }
}

bool BillingTableModel::HasScheduledItem() const {
    for (const auto& it : m_items)
        if (it.is_scheduled > 0) return true;
    return false;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void BillingTableModel::RecomputeLine(BillItemRow& it) const {
    gst::LineResult r = gst::compute_line(
        it.mrp, it.quantity, it.discount_pct, it.gst_rate, false);
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
    time_t t = time(nullptr);
    struct tm* lt = localtime(&t);
    lt->tm_mon += 3;
    if (lt->tm_mon >= 12) { lt->tm_year++; lt->tm_mon -= 12; }
    char threshold[8];
    std::snprintf(threshold, sizeof(threshold), "%04d-%02d",
                  lt->tm_year + 1900, lt->tm_mon + 1);
    return expiry_ym <= threshold && !IsExpired(expiry_ym);
}
