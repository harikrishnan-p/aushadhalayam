#pragma once
// =============================================================================
// MainFrame.h  —  Top-level frame: sidebar + page book
// =============================================================================

#include <wx/wx.h>
#include <wx/simplebook.h>   // wxSimplebook
#include <wx/statusbr.h>
#include <string>
#include <vector>

#include "WorkerThread.h"
#include "SidebarPanel.h"
#include "PrinterManager.h"
#include "Pages.h"

// Forward declarations
class DashboardPage;
class BillingPage;
class InventoryPage;
class MedicinesPage;
class PurchasePage;
class SalesPage;
class CustomersPage;
class SettingsPage;

enum PageId {
    PAGE_DASHBOARD  = 0,
    PAGE_BILLING    = 1,
    PAGE_INVENTORY  = 2,
    PAGE_MEDICINES  = 3,
    PAGE_PURCHASE   = 4,
    PAGE_SALES      = 5,
    PAGE_CUSTOMERS  = 6,
    PAGE_SETTINGS   = 7,
    PAGE_COUNT      = 8,
};

enum {
    ID_TIMER_SYNC = wxID_HIGHEST + 500,
};

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& db_path,
              const std::string& device_id,
              const PrintConfig& print_cfg);
    ~MainFrame();

private:
    PharmacyWorkerThread* m_worker;
    std::string           m_device_id;
    wxTimer               m_sync_timer;
    PrinterManager*       m_printer;

    SidebarPanel*  m_sidebar;
    wxSimplebook*  m_book;

    // ── Topbar controls ───────────────────────────────────────────────────────
    wxPanel*       m_topbar;
    wxStaticText*  m_topbar_title;
    wxStaticText*  m_topbar_sync;

    // ── Pages ─────────────────────────────────────────────────────────────────
    BasePage*      m_pages[PAGE_COUNT];
    int            m_current_page;

    static const char* kPageTitles[PAGE_COUNT];

    void BuildLayout();
    void SwitchToPage(int page_id);

    void OnSidebarNav (wxCommandEvent&);
    void OnSyncTimer  (wxTimerEvent&);
    void OnClose      (wxCloseEvent&);
    void OnDbResult   (wxCommandEvent&);
    void OnDbError    (wxCommandEvent&);

    wxDECLARE_EVENT_TABLE();
};
