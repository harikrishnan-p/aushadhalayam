#pragma once
// =============================================================================
// GstEngine.h  —  Header-only GST computation for Indian pharmacy retail
//
// Key rule: in Indian retail, MRP printed on the package is GST-INCLUSIVE.
// Taxable value is back-calculated from the MRP amount.
//
//   line_total    = mrp × qty × (1 − discount_pct/100)
//   taxable_value = line_total / (1 + gst_rate/100)
//   gst_amount    = line_total − taxable_value
//   intra-state  → cgst = sgst = gst_amount / 2,  igst = 0
//   inter-state  → igst = gst_amount,              cgst = sgst = 0
// =============================================================================

#include <cmath>
#include <string>

namespace gst {

struct LineResult {
    double unit_price;      // mrp × (1 − discount_pct/100) — GST-inclusive
    double line_total;      // unit_price × qty
    double taxable_value;   // line_total / (1 + rate/100)
    double cgst_amount;
    double sgst_amount;
    double igst_amount;
    double total_gst;
};

struct BillTotals {
    double total_amount;
    double discount_amount;
    double taxable_amount;
    double cgst_amount;
    double sgst_amount;
    double igst_amount;
    double total_gst_amount;
    double grand_total;
};

// Round to two decimal places (standard Indian accounting rounding)
inline double round2(double v) {
    return std::floor(v * 100.0 + 0.5) / 100.0;
}

inline LineResult compute_line(double mrp,
                               int    qty,
                               double discount_pct,
                               double gst_rate,
                               bool   is_interstate)
{
    LineResult r{};
    r.unit_price    = round2(mrp * (1.0 - discount_pct / 100.0));
    r.line_total    = round2(r.unit_price * static_cast<double>(qty));
    r.taxable_value = round2(r.line_total / (1.0 + gst_rate / 100.0));
    double gst      = round2(r.line_total - r.taxable_value);
    if (is_interstate) {
        r.igst_amount = gst;
    } else {
        r.cgst_amount = round2(gst / 2.0);
        r.sgst_amount = round2(gst - r.cgst_amount); // absorb rounding remainder
    }
    r.total_gst = round2(r.cgst_amount + r.sgst_amount + r.igst_amount);
    return r;
}

// Validate that a GST rate is a legally recognised Indian pharma slab
inline bool is_valid_gst_rate(double rate) {
    static const double kSlabs[] = {0.0, 0.1, 0.25, 1.5, 3.0, 5.0, 12.0, 18.0, 28.0};
    for (double s : kSlabs) {
        if (std::abs(rate - s) < 1e-9) return true;
    }
    return false;
}

// Human-readable HSN chapter description (top-level for common pharma codes)
inline std::string hsn_description(const std::string& hsn) {
    if (hsn.size() >= 2) {
        const std::string ch = hsn.substr(0, 2);
        if (ch == "30") return "Pharmaceutical Products";
        if (ch == "33") return "Essential Oils & Cosmetics";
        if (ch == "38") return "Miscellaneous Chemical Products";
    }
    return "Other";
}

} // namespace gst
