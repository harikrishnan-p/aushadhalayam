// =============================================================================
// main.cpp  —  wxApp entry point for Aushadhalayam Legacy POS
//
// Subsystem: Windows GUI (no console window).
// Compiled with: -mwindows -Wl,--subsystem,windows:5.01
// Target: Windows XP SP3 through Windows 11 (32-bit executable).
// =============================================================================

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/fileconf.h>
#include <windows.h>
#include <cstdio>

#include "MainFrame.h"
#include "PrinterManager.h"

// Include the SQLite amalgamation compilation unit here so the linker sees it
// via the sqlite3_static CMake target.  Nothing extra needed in this file.

// ─────────────────────────────────────────────────────────────────────────────
// Utility: load or generate a persistent device UUID stored in a local INI file
// ─────────────────────────────────────────────────────────────────────────────
static std::string GetOrCreateDeviceId(const wxString& data_dir) {
    wxString cfg_file = data_dir + wxFILE_SEP_PATH + "device.cfg";
    wxFileConfig cfg("", "", cfg_file, "", wxCONFIG_USE_LOCAL_FILE);
    wxString id;
    if (cfg.Read("device_id", &id) && !id.IsEmpty())
        return std::string(id.mb_str());

    // Generate a simple time-based pseudo-UUID (no cryptographic requirement)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long ts = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32)
                           | ft.dwLowDateTime;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "device-%016llx", ts);
    cfg.Write("device_id", wxString::FromAscii(buf));
    cfg.Flush();
    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// Load printer configuration from INI
// ─────────────────────────────────────────────────────────────────────────────
static PrintConfig LoadPrintConfig(const wxString& data_dir) {
    wxString cfg_file = data_dir + wxFILE_SEP_PATH + "printer.cfg";
    wxFileConfig cfg("", "", cfg_file, "", wxCONFIG_USE_LOCAL_FILE);

    PrintConfig pc{};
    wxString mode;
    cfg.Read("mode", &mode, "GDI");
    pc.mode = (mode == "COM") ? PrintMode::ESCPOS_COM : PrintMode::GDI_SPOOLER;

    wxString name;
    cfg.Read("name", &name, "");
    pc.printer_name  = std::string(name.mb_str());
    pc.baud_rate     = cfg.ReadLong("baud_rate", 9600);
    pc.paper_width_mm= static_cast<int>(cfg.ReadLong("paper_width_mm", 80));
    pc.cut_paper     = cfg.ReadBool("cut_paper", true);
    return pc;
}

// ─────────────────────────────────────────────────────────────────────────────
// wxApp subclass
// ─────────────────────────────────────────────────────────────────────────────
class PosApp : public wxApp {
public:
    bool OnInit() override {
        // wxWidgets image handlers — not strictly needed for POS but safe to init
        wxInitAllImageHandlers();

        // Data directory: same folder as the executable
        wxString data_dir = wxStandardPaths::Get().GetExecutablePath();
        wxFileName fn(data_dir);
        data_dir = fn.GetPath();

        wxString db_path = data_dir + wxFILE_SEP_PATH + "pharmacy.db";
        std::string device_id = GetOrCreateDeviceId(data_dir);
        PrintConfig print_cfg = LoadPrintConfig(data_dir);

        // Apply schema on first launch (idempotent — uses IF NOT EXISTS)
        // In production, read schema.sql from the bundle resources.
        // For now, the MainFrame → WorkerThread will handle DB initialisation.

        MainFrame* frame = new MainFrame(db_path, device_id, print_cfg);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(PosApp);
