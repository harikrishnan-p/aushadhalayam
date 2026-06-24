#pragma once
// =============================================================================
// PrinterManager.h  —  Thermal receipt printing for Windows XP SP3 – Win11
//
// Two backends, selected at runtime via PrintConfig::mode:
//
//   GDI_SPOOLER  —  Routes through the Windows Print Spooler (CreateDC /
//                   StartDoc / TextOut / EndDoc).  Works with any printer
//                   driver installed in Windows — parallel, USB, network.
//                   Best for formatted A4/80mm receipts with GDI layout.
//
//   ESCPOS_COM   —  Writes raw ESC/POS byte sequences directly to a serial
//                   COM port.  Zero driver required; fastest path for embedded
//                   thermal printers (Epson TM series, etc.) connected via
//                   DB9/USB-Serial.
//
// Both backends are synchronous and blocking; call from the GUI thread or a
// dedicated print worker thread (never from the SQLite worker thread).
// =============================================================================

#include <string>
#include <vector>

// Forward-declare so wx headers are not pulled in here
struct BillItemRow;

// ─────────────────────────────────────────────────────────────────────────────
// ReceiptData  —  all information needed to render a receipt
// ─────────────────────────────────────────────────────────────────────────────
struct ReceiptData {
    std::string pharmacy_name;
    std::string pharmacy_address;
    std::string pharmacy_gstin;
    std::string pharmacy_dl;          // Drug License number
    std::string pharmacy_phone;

    std::string bill_number;
    std::string bill_date;
    std::string cashier_name;
    std::string customer_name;
    std::string payment_mode;

    std::vector<BillItemRow> items;   // line items from BillingTableModel

    double total_amount;
    double discount_amount;
    double taxable_amount;
    double cgst_amount;
    double sgst_amount;
    double grand_total;
};

// ─────────────────────────────────────────────────────────────────────────────
// PrintConfig  —  loaded from INI/registry at startup
// ─────────────────────────────────────────────────────────────────────────────
enum class PrintMode {
    GDI_SPOOLER,   // route through Windows Print Spooler
    ESCPOS_COM,    // raw ESC/POS bytes to a COM port
};

struct PrintConfig {
    PrintMode   mode;
    std::string printer_name;  // GDI: Windows printer name; COM: "COM1" … "COM9"
    int         baud_rate;     // COM mode only (default 9600)
    int         paper_width_mm;// 58 or 80 mm
    bool        cut_paper;     // send full-cut command after receipt
};

// ─────────────────────────────────────────────────────────────────────────────
// PrinterManager
// ─────────────────────────────────────────────────────────────────────────────
class PrinterManager {
public:
    explicit PrinterManager(const PrintConfig& cfg);

    // Print a receipt; returns true on success.
    // error_out receives a human-readable description on failure.
    bool PrintReceipt(const ReceiptData& data, std::string& error_out);

    // List installed Windows printers (GDI mode helper for settings UI)
    static std::vector<std::string> EnumeratePrinters();

private:
    PrintConfig m_cfg;

    // GDI path
    bool PrintViaGdi    (const ReceiptData& data, std::string& err);
    void RenderGdiPage  (void* hdc, const ReceiptData& data);

    // ESC/POS path
    bool PrintViaEscPos (const ReceiptData& data, std::string& err);
    void BuildEscPosDoc (const ReceiptData& data, std::vector<unsigned char>& out);

    // ESC/POS command constants
    static void AppendInit       (std::vector<unsigned char>& b);
    static void AppendAlign      (std::vector<unsigned char>& b, int align); // 0=L 1=C 2=R
    static void AppendBold       (std::vector<unsigned char>& b, bool on);
    static void AppendDoubleWidth(std::vector<unsigned char>& b, bool on);
    static void AppendText       (std::vector<unsigned char>& b, const std::string& s);
    static void AppendLineFeed   (std::vector<unsigned char>& b, int n = 1);
    static void AppendCutPaper   (std::vector<unsigned char>& b);
    static void AppendRuler      (std::vector<unsigned char>& b, int width_chars);

    PrinterManager(const PrinterManager&) = delete;
    PrinterManager& operator=(const PrinterManager&) = delete;
};
