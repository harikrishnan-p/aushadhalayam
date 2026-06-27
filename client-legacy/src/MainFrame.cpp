// =============================================================================
// MainFrame.cpp  —  Top-level frame: sidebar + page book
// =============================================================================

#include "MainFrame.h"
#include "AppColors.h"
#include "WorkerThread.h"
#include "PrinterManager.h"

// Pages
#include "DashboardPage.h"
#include "BillingPage.h"
#include "InventoryPage.h"
#include "MedicinesPage.h"
#include "PurchasePage.h"
#include "SalesPage.h"
#include "CustomersPage.h"
#include "SettingsPage.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/panel.h>
#include <wx/msgdlg.h>
#include <wx/simplebook.h>

// ─────────────────────────────────────────────────────────────────────────────

const char* MainFrame::kPageTitles[PAGE_COUNT] = {
    "Dashboard", "Billing", "Inventory", "Medicines",
    "Purchase", "Sales", "Customers", "Settings",
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_COMMAND(wxID_ANY, wxEVT_SIDEBAR_NAV, MainFrame::OnSidebarNav)
    EVT_TIMER  (ID_TIMER_SYNC,               MainFrame::OnSyncTimer)
    EVT_CLOSE  (                             MainFrame::OnClose)
    EVT_COMMAND(wxID_ANY, wxEVT_DB_RESULT,   MainFrame::OnDbResult)
    EVT_COMMAND(wxID_ANY, wxEVT_DB_ERROR,    MainFrame::OnDbError)
wxEND_EVENT_TABLE()

// ─────────────────────────────────────────────────────────────────────────────

MainFrame::MainFrame(const wxString& db_path,
                     const std::string& device_id,
                     const PrintConfig& print_cfg)
    : wxFrame(nullptr, wxID_ANY, "Aushadhalayam - Pharmacy POS",
              wxDefaultPosition, wxSize(1366, 768))
    , m_worker(nullptr)
    , m_device_id(device_id)
    , m_sync_timer(this, ID_TIMER_SYNC)
    , m_printer(nullptr)
    , m_sidebar(nullptr)
    , m_book(nullptr)
    , m_topbar(nullptr)
    , m_topbar_title(nullptr)
    , m_topbar_sync(nullptr)
    , m_current_page(PAGE_DASHBOARD)
{
    for (int i = 0; i < PAGE_COUNT; ++i) m_pages[i] = nullptr;

    m_printer = new PrinterManager(print_cfg);

    // ── Start worker thread ──────────────────────────────────────────────────
    m_worker = new PharmacyWorkerThread(this, db_path, device_id);
    if (m_worker->Create() != wxTHREAD_NO_ERROR) {
        wxMessageBox("Failed to create database thread", "Fatal Error",
                     wxICON_ERROR | wxOK);
        Close(true);
        return;
    }
    m_worker->Run();

    BuildLayout();

    m_sync_timer.Start(30000);

    SetMinSize(wxSize(1024, 600));
    SetBackgroundColour(clr::Bg());
    Centre();
    Show(true);

    // Show dashboard first
    SwitchToPage(PAGE_DASHBOARD);
}

MainFrame::~MainFrame() {
    delete m_printer;
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────────

void MainFrame::BuildLayout() {
    wxPanel* root_panel = new wxPanel(this);
    root_panel->SetBackgroundColour(clr::Bg());

    // ── Outer sizer: sidebar | right column ──────────────────────────────────
    wxBoxSizer* outer = new wxBoxSizer(wxHORIZONTAL);

    std::vector<NavEntry> nav_entries = {
        {"Dashboard", "MAIN",    PAGE_DASHBOARD,  ICON_DASHBOARD},
        {"Billing",   "",        PAGE_BILLING,    ICON_BILLING},
        {"Inventory", "STORE",   PAGE_INVENTORY,  ICON_INVENTORY},
        {"Medicines", "",        PAGE_MEDICINES,  ICON_MEDICINES},
        {"Purchase",  "",        PAGE_PURCHASE,   ICON_PURCHASE},
        {"Sales",     "REPORTS", PAGE_SALES,      ICON_SALES},
        {"Customers", "",        PAGE_CUSTOMERS,  ICON_CUSTOMERS},
        {"Settings",  "SYSTEM",  PAGE_SETTINGS,   ICON_SETTINGS},
    };
    m_sidebar = new SidebarPanel(root_panel, nav_entries, PAGE_DASHBOARD);
    outer->Add(m_sidebar, 0, wxEXPAND);

    // ── Right column: topbar + page book ─────────────────────────────────────
    wxBoxSizer* right_col = new wxBoxSizer(wxVERTICAL);

    // Topbar
    m_topbar = new wxPanel(root_panel);
    m_topbar->SetBackgroundColour(clr::Surface());
    m_topbar->SetMinSize(wxSize(-1, sz::TopbarH));
    {
        wxBoxSizer* tb = new wxBoxSizer(wxHORIZONTAL);
        tb->AddSpacer(sz::CardPad);

        m_topbar_title = new wxStaticText(m_topbar, wxID_ANY, "Dashboard");
        wxFont tf; tf.SetPointSize(sz::FontPt + 2); tf.MakeBold();
        m_topbar_title->SetFont(tf);
        m_topbar_title->SetForegroundColour(clr::Text());
        tb->Add(m_topbar_title, 1, wxALIGN_CENTER_VERTICAL);

        m_topbar_sync = new wxStaticText(m_topbar, wxID_ANY, "Sync: --");
        wxFont sf; sf.SetPointSize(9);
        m_topbar_sync->SetFont(sf);
        m_topbar_sync->SetForegroundColour(clr::Text2());
        tb->Add(m_topbar_sync, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, sz::CardPad);

        m_topbar->SetSizer(tb);
    }
    right_col->Add(m_topbar, 0, wxEXPAND);

    // Separator line
    wxPanel* sep = new wxPanel(root_panel);
    sep->SetBackgroundColour(clr::Border());
    sep->SetMinSize(wxSize(-1, 1));
    right_col->Add(sep, 0, wxEXPAND);

    // Page book
    m_book = new wxSimplebook(root_panel);
    m_book->SetBackgroundColour(clr::Bg());

    // Create pages in page-id order
    auto* dash = new DashboardPage (m_book, m_worker); m_pages[PAGE_DASHBOARD] = dash;
    auto* bill = new BillingPage   (m_book, m_worker, m_device_id); m_pages[PAGE_BILLING]   = bill;
    auto* inv  = new InventoryPage (m_book, m_worker); m_pages[PAGE_INVENTORY] = inv;
    auto* med  = new MedicinesPage (m_book, m_worker); m_pages[PAGE_MEDICINES] = med;
    auto* pur  = new PurchasePage  (m_book, m_worker); m_pages[PAGE_PURCHASE]  = pur;
    auto* sal  = new SalesPage     (m_book, m_worker); m_pages[PAGE_SALES]     = sal;
    auto* cust = new CustomersPage (m_book, m_worker); m_pages[PAGE_CUSTOMERS] = cust;
    auto* sett = new SettingsPage  (m_book, m_worker); m_pages[PAGE_SETTINGS]  = sett;

    m_book->AddPage(dash, "Dashboard");
    m_book->AddPage(bill, "Billing");
    m_book->AddPage(inv,  "Inventory");
    m_book->AddPage(med,  "Medicines");
    m_book->AddPage(pur,  "Purchase");
    m_book->AddPage(sal,  "Sales");
    m_book->AddPage(cust, "Customers");
    m_book->AddPage(sett, "Settings");

    right_col->Add(m_book, 1, wxEXPAND);

    outer->Add(right_col, 1, wxEXPAND);
    root_panel->SetSizer(outer);

    wxBoxSizer* frame_sizer = new wxBoxSizer(wxVERTICAL);
    frame_sizer->Add(root_panel, 1, wxEXPAND);
    SetSizer(frame_sizer);
}

void MainFrame::SwitchToPage(int page_id) {
    if (page_id < 0 || page_id >= PAGE_COUNT) return;
    m_current_page = page_id;
    m_book->SetSelection(page_id);
    m_sidebar->SetActivePage(page_id);
    m_topbar_title->SetLabel(wxString::FromUTF8(kPageTitles[page_id]));
    if (m_pages[page_id]) m_pages[page_id]->OnPageShown();
}

// ─────────────────────────────────────────────────────────────────────────────
// Event handlers
// ─────────────────────────────────────────────────────────────────────────────

void MainFrame::OnSidebarNav(wxCommandEvent& e) {
    SwitchToPage(e.GetInt());
}

void MainFrame::OnSyncTimer(wxTimerEvent& /*e*/) {
    DbTask t;
    t.type       = DbCommandType::CMD_FLUSH_SYNC;
    t.payload    = "{}";
    t.request_id = -1;   // not tracked; we only read the response for status
    m_worker->PostTask(t);
}

void MainFrame::OnClose(wxCloseEvent& e) {
    m_sync_timer.Stop();
    if (m_worker) {
        DbTask q;
        q.type       = DbCommandType::CMD_QUIT;
        q.request_id = -1;
        m_worker->PostTask(q);
        m_worker->Wait();
        delete m_worker;
        m_worker = nullptr;
    }
    e.Skip();
}

void MainFrame::OnDbResult(wxCommandEvent& e) {
    long long   req_id = static_cast<long long>(e.GetExtraLong());
    std::string json   = std::string(e.GetString().utf8_str());

    // Let each page claim its own result first
    for (int i = 0; i < PAGE_COUNT; ++i) {
        if (m_pages[i] && m_pages[i]->TryHandleDbResult(req_id, json))
            return;
    }

    // Unclaimed: might be a sync status response
    if (json.find("pending_sync_count") != std::string::npos) {
        std::string cnt = jsonutil::extract_str(json, "pending_sync_count");
        m_topbar_sync->SetLabel("Sync pending: " + wxString::FromUTF8(cnt.c_str()));
    }
}

void MainFrame::OnDbError(wxCommandEvent& e) {
    long long   req_id = static_cast<long long>(e.GetExtraLong());
    std::string msg    = std::string(e.GetString().utf8_str());

    for (int i = 0; i < PAGE_COUNT; ++i) {
        if (m_pages[i] && m_pages[i]->TryHandleDbError(req_id, msg))
            return;
    }

    // Unclaimed error
    wxMessageBox(wxString::FromUTF8(msg.c_str()), "Database Error",
                 wxICON_ERROR | wxOK, this);
}
