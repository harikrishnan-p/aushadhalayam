// =============================================================================
// PrinterManager.cpp  —  GDI Spooler + ESC/POS thermal printer backends
//
// Windows XP SP3 compatible: only Win32 APIs present since Windows 2000 are used.
// =============================================================================

#include "PrinterManager.h"
#include "BillingGrid.h"       // BillItemRow

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>

PrinterManager::PrinterManager(const PrintConfig& cfg) : m_cfg(cfg) {}

bool PrinterManager::PrintReceipt(const ReceiptData& data, std::string& error_out) {
    switch (m_cfg.mode) {
        case PrintMode::GDI_SPOOLER: return PrintViaGdi   (data, error_out);
        case PrintMode::ESCPOS_COM:  return PrintViaEscPos (data, error_out);
        default:
            error_out = "Unknown printer mode";
            return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GDI Spooler backend
// ─────────────────────────────────────────────────────────────────────────────

bool PrinterManager::PrintViaGdi(const ReceiptData& data, std::string& err) {
    HDC hDC = CreateDCA("WINSPOOL",
                        m_cfg.printer_name.c_str(),
                        nullptr, nullptr);
    if (!hDC) {
        err = "CreateDC failed for printer: " + m_cfg.printer_name;
        return false;
    }

    DOCINFOA di = {};
    di.cbSize      = sizeof(DOCINFOA);
    di.lpszDocName = "Aushadhalayam Receipt";

    if (StartDocA(hDC, &di) <= 0) {
        err = "StartDoc failed";
        DeleteDC(hDC);
        return false;
    }
    StartPage(hDC);
    RenderGdiPage(hDC, data);
    EndPage(hDC);
    EndDoc(hDC);
    DeleteDC(hDC);
    return true;
}

void PrinterManager::RenderGdiPage(void* vhdc, const ReceiptData& data) {
    HDC hDC = reinterpret_cast<HDC>(vhdc);

    // Dots-per-inch of the printer
    int dpi_x = GetDeviceCaps(hDC, LOGPIXELSX);
    int dpi_y = GetDeviceCaps(hDC, LOGPIXELSY);

    // 1 mm = dpi/25.4 pixels
    auto mm2px_x = [&](double mm){ return static_cast<int>(mm * dpi_x / 25.4); };
    auto mm2px_y = [&](double mm){ return static_cast<int>(mm * dpi_y / 25.4); };

    int margin_x = mm2px_x(5);
    int margin_y = mm2px_y(5);
    int line_h   = mm2px_y(5);
    int y = margin_y;

    // Fonts
    HFONT font_normal = CreateFontA(
        -MulDiv(9, dpi_y, 72), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_MODERN, "Courier New");
    HFONT font_bold = CreateFontA(
        -MulDiv(10, dpi_y, 72), 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_MODERN, "Courier New");

    auto text_out = [&](int x, const std::string& s, HFONT f) {
        SelectObject(hDC, f);
        TextOutA(hDC, x, y, s.c_str(), static_cast<int>(s.size()));
    };

    int page_w = GetDeviceCaps(hDC, HORZRES);
    int col_w  = (m_cfg.paper_width_mm == 80) ? 48 : 32; // chars per line

    auto centre = [&](const std::string& s) {
        int pad = (col_w - static_cast<int>(s.size())) / 2;
        if (pad < 0) pad = 0;
        return std::string(pad, ' ') + s;
    };

    auto ruler = [&](char c) {
        return std::string(col_w, c);
    };

    // ── Header ───────────────────────────────────────────────────────────────
    text_out(margin_x, centre(data.pharmacy_name), font_bold);
    y += line_h;
    text_out(margin_x, centre(data.pharmacy_address), font_normal);
    y += line_h;
    text_out(margin_x, centre("GSTIN: " + data.pharmacy_gstin), font_normal);
    y += line_h;
    text_out(margin_x, centre("DL: " + data.pharmacy_dl), font_normal);
    y += line_h;
    text_out(margin_x, ruler('='), font_normal); y += line_h;

    char buf[256];
    std::snprintf(buf, sizeof(buf), "Bill: %-20s %s",
                  data.bill_number.c_str(), data.bill_date.c_str());
    text_out(margin_x, buf, font_normal); y += line_h;
    if (!data.customer_name.empty()) {
        std::snprintf(buf, sizeof(buf), "Patient: %s", data.customer_name.c_str());
        text_out(margin_x, buf, font_normal); y += line_h;
    }
    text_out(margin_x, ruler('-'), font_normal); y += line_h;

    // ── Column header ─────────────────────────────────────────────────────────
    std::snprintf(buf, sizeof(buf), "%-20s %3s %6s %6s",
                  "Medicine", "Qty", "Price", "Total");
    text_out(margin_x, buf, font_bold); y += line_h;
    text_out(margin_x, ruler('-'), font_normal); y += line_h;

    // ── Items ─────────────────────────────────────────────────────────────────
    for (const auto& it : data.items) {
        std::string name = it.product_name;
        if (name.size() > 20) name = name.substr(0, 19) + ".";
        std::snprintf(buf, sizeof(buf), "%-20s %3d %6.2f %6.2f",
                      name.c_str(), it.quantity, it.unit_price, it.line_total);
        text_out(margin_x, buf, font_normal); y += line_h;

        // Batch / expiry sub-line
        std::snprintf(buf, sizeof(buf), "  Batch:%-10s Exp:%s",
                      it.batch_number.c_str(), it.expiry_date.c_str());
        text_out(margin_x, buf, font_normal); y += line_h;
    }

    text_out(margin_x, ruler('-'), font_normal); y += line_h;

    // ── Totals ────────────────────────────────────────────────────────────────
    std::snprintf(buf, sizeof(buf), "%-28s %7.2f", "Taxable Amount:", data.taxable_amount);
    text_out(margin_x, buf, font_normal); y += line_h;
    std::snprintf(buf, sizeof(buf), "%-28s %7.2f", "CGST:", data.cgst_amount);
    text_out(margin_x, buf, font_normal); y += line_h;
    std::snprintf(buf, sizeof(buf), "%-28s %7.2f", "SGST:", data.cgst_amount);
    text_out(margin_x, buf, font_normal); y += line_h;
    if (data.discount_amount > 0) {
        std::snprintf(buf, sizeof(buf), "%-28s %7.2f", "Discount:", data.discount_amount);
        text_out(margin_x, buf, font_normal); y += line_h;
    }
    text_out(margin_x, ruler('='), font_normal); y += line_h;
    std::snprintf(buf, sizeof(buf), "%-28s %7.2f", "GRAND TOTAL:", data.grand_total);
    text_out(margin_x, buf, font_bold); y += line_h;
    text_out(margin_x, ruler('='), font_normal); y += line_h;

    std::snprintf(buf, sizeof(buf), "Payment: %s", data.payment_mode.c_str());
    text_out(margin_x, buf, font_normal); y += line_h;
    y += line_h;
    text_out(margin_x, centre("Thank you! Get well soon."), font_normal);

    DeleteObject(font_normal);
    DeleteObject(font_bold);
}

// ─────────────────────────────────────────────────────────────────────────────
// ESC/POS raw COM port backend
// ─────────────────────────────────────────────────────────────────────────────

bool PrinterManager::PrintViaEscPos(const ReceiptData& data, std::string& err) {
    // Open COM port with CreateFile (XP-compatible)
    std::string port_path = "\\\\.\\" + m_cfg.printer_name; // e.g. "\\.\COM3"
    HANDLE hCOM = CreateFileA(port_path.c_str(),
                              GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    if (hCOM == INVALID_HANDLE_VALUE) {
        err = "Cannot open " + m_cfg.printer_name +
              " (Error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    // Configure serial port (typical thermal printer: 9600 8N1)
    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hCOM, &dcb)) {
        CloseHandle(hCOM); err = "GetCommState failed"; return false;
    }
    dcb.BaudRate = m_cfg.baud_rate > 0 ? static_cast<DWORD>(m_cfg.baud_rate) : CBR_9600;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;
    if (!SetCommState(hCOM, &dcb)) {
        CloseHandle(hCOM); err = "SetCommState failed"; return false;
    }

    COMMTIMEOUTS timeouts = {};
    timeouts.WriteTotalTimeoutConstant   = 2000;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    SetCommTimeouts(hCOM, &timeouts);

    // Build ESC/POS document
    std::vector<unsigned char> doc;
    BuildEscPosDoc(data, doc);

    DWORD written = 0;
    BOOL ok = WriteFile(hCOM,
                        doc.data(),
                        static_cast<DWORD>(doc.size()),
                        &written, nullptr);
    CloseHandle(hCOM);

    if (!ok || written != doc.size()) {
        err = "WriteFile to printer port failed";
        return false;
    }
    return true;
}

void PrinterManager::BuildEscPosDoc(const ReceiptData& data,
                                    std::vector<unsigned char>& b) {
    int col_w = (m_cfg.paper_width_mm == 80) ? 48 : 32;

    auto pad_right = [](const std::string& s, size_t w) {
        if (s.size() >= w) return s.substr(0, w);
        return s + std::string(w - s.size(), ' ');
    };
    auto pad_left = [](const std::string& s, size_t w) {
        if (s.size() >= w) return s.substr(0, w);
        return std::string(w - s.size(), ' ') + s;
    };

    AppendInit(b);

    // ── Header ───────────────────────────────────────────────────────────────
    AppendAlign(b, 1);    // centre
    AppendBold(b, true);
    AppendDoubleWidth(b, true);
    AppendText(b, data.pharmacy_name); AppendLineFeed(b);
    AppendDoubleWidth(b, false);
    AppendBold(b, false);
    AppendText(b, data.pharmacy_address); AppendLineFeed(b);
    AppendText(b, "GSTIN: " + data.pharmacy_gstin); AppendLineFeed(b);
    AppendText(b, "DL: "    + data.pharmacy_dl);    AppendLineFeed(b);
    AppendRuler(b, col_w);

    AppendAlign(b, 0);    // left
    AppendText(b, "Bill: " + data.bill_number); AppendLineFeed(b);
    AppendText(b, "Date: " + data.bill_date);   AppendLineFeed(b);
    if (!data.customer_name.empty()) {
        AppendText(b, "Patient: " + data.customer_name); AppendLineFeed(b);
    }
    AppendRuler(b, col_w);

    // Column header
    AppendBold(b, true);
    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "%-20s %3s %6s %6s", "Medicine","Qty","Price","Total");
    AppendText(b, hdr); AppendLineFeed(b);
    AppendBold(b, false);
    AppendRuler(b, col_w);

    // Items
    for (const auto& it : data.items) {
        std::string name = it.product_name.size() > 20
                         ? it.product_name.substr(0, 19) + "."
                         : it.product_name;
        char line[64];
        std::snprintf(line, sizeof(line), "%-20s %3d %6.2f %6.2f",
                      name.c_str(), it.quantity, it.unit_price, it.line_total);
        AppendText(b, line); AppendLineFeed(b);

        char sub[48];
        std::snprintf(sub, sizeof(sub), "  Batch:%-8s Exp:%s",
                      it.batch_number.c_str(), it.expiry_date.c_str());
        AppendText(b, sub); AppendLineFeed(b);
    }
    AppendRuler(b, col_w);

    // Totals
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%-30s%7.2f", "Taxable:", data.taxable_amount);
    AppendText(b, buf); AppendLineFeed(b);
    std::snprintf(buf, sizeof(buf), "%-30s%7.2f", "CGST:", data.cgst_amount);
    AppendText(b, buf); AppendLineFeed(b);
    std::snprintf(buf, sizeof(buf), "%-30s%7.2f", "SGST:", data.cgst_amount);
    AppendText(b, buf); AppendLineFeed(b);
    AppendRuler(b, col_w);
    AppendBold(b, true);
    AppendDoubleWidth(b, true);
    std::snprintf(buf, sizeof(buf), "TOTAL: INR %.2f", data.grand_total);
    AppendText(b, buf); AppendLineFeed(b);
    AppendDoubleWidth(b, false);
    AppendBold(b, false);
    AppendRuler(b, col_w);

    std::snprintf(buf, sizeof(buf), "Payment: %s", data.payment_mode.c_str());
    AppendText(b, buf); AppendLineFeed(b, 2);
    AppendAlign(b, 1);
    AppendText(b, "Thank you!"); AppendLineFeed(b, 4);

    if (m_cfg.cut_paper) AppendCutPaper(b);
}

// ── ESC/POS command primitives ────────────────────────────────────────────────

void PrinterManager::AppendInit(std::vector<unsigned char>& b) {
    b.push_back(0x1B); b.push_back(0x40); // ESC @  — initialize printer
}

void PrinterManager::AppendAlign(std::vector<unsigned char>& b, int align) {
    b.push_back(0x1B); b.push_back(0x61); b.push_back(static_cast<unsigned char>(align));
}

void PrinterManager::AppendBold(std::vector<unsigned char>& b, bool on) {
    b.push_back(0x1B); b.push_back(0x45); b.push_back(on ? 1 : 0);
}

void PrinterManager::AppendDoubleWidth(std::vector<unsigned char>& b, bool on) {
    // ESC ! n — select print mode; bit 5 = double-width
    b.push_back(0x1B); b.push_back(0x21); b.push_back(on ? 0x20 : 0x00);
}

void PrinterManager::AppendText(std::vector<unsigned char>& b, const std::string& s) {
    for (unsigned char c : s) b.push_back(c);
}

void PrinterManager::AppendLineFeed(std::vector<unsigned char>& b, int n) {
    for (int i = 0; i < n; ++i) b.push_back(0x0A);
}

void PrinterManager::AppendCutPaper(std::vector<unsigned char>& b) {
    b.push_back(0x1D); b.push_back(0x56); b.push_back(0x41); // GS V A — full cut
    b.push_back(0x00);
}

void PrinterManager::AppendRuler(std::vector<unsigned char>& b, int w) {
    AppendText(b, std::string(w, '-'));
    b.push_back(0x0A);
}

// ─────────────────────────────────────────────────────────────────────────────
// Enumerate installed printers (GDI helper)
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::string> PrinterManager::EnumeratePrinters() {
    std::vector<std::string> result;

    DWORD needed = 0, returned = 0;
    EnumPrintersA(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
                  nullptr, 2, nullptr, 0, &needed, &returned);

    if (needed == 0) return result;

    std::vector<BYTE> buf(needed);
    if (!EnumPrintersA(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
                       nullptr, 2, buf.data(), needed, &needed, &returned))
        return result;

    PRINTER_INFO_2A* info = reinterpret_cast<PRINTER_INFO_2A*>(buf.data());
    for (DWORD i = 0; i < returned; ++i) {
        if (info[i].pPrinterName)
            result.push_back(info[i].pPrinterName);
    }
    return result;
}
