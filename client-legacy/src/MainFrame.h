#pragma once
// =============================================================================
// MainFrame.h  —  Primary wxFrame for the legacy POS UI
//
// Layout overview (all within a single wxFrame):
//
//  ┌─[Toolbar: Search | Checkout | Cancel | Print | Sync Status]──────────┐
//  │ ┌─[Left: Search + Lookup]──────┐ ┌─[Right: Bill Summary]───────────┐ │
//  │ │ SKU/Name: [__________] [Find] │ │ Patient: [__________________]   │ │
//  │ │ Barcode:  [auto-focus input ] │ │ Items: 0   Qty: 0              │ │
//  │ │                               │ │ Disc %: [___] Bill Disc: [___] │ │
//  │ └───────────────────────────────┘ └────────────────────────────────┘ │
//  │ ┌─[Billing Grid — wxGrid with BillingTableModel]────────────────────┐ │
//  │ │ # │ Medicine        │ Batch │ Expiry │ Qty │ MRP │ Disc │ Total  │ │
//  │ │ 1 │ Paracetamol 500 │ B001  │ 06/26  │  2  │12.00│  0%  │ 24.00 │ │
//  │ └───────────────────────────────────────────────────────────────────┘ │
//  │ CGST: ₹0.00   SGST: ₹0.00   Grand Total: ₹0.00  [CHECKOUT] [CLEAR]  │
//  └────────────────────────────────────────────────────────────────────────┘
// =============================================================================

#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/toolbar.h>
#include <wx/statusbr.h>
#include <string>

#include "WorkerThread.h"
#include "BillingGrid.h"
#include "PrinterManager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Event table IDs
// ─────────────────────────────────────────────────────────────────────────────
enum {
    ID_BTN_SEARCH    = wxID_HIGHEST + 1,
    ID_BTN_CHECKOUT,
    ID_BTN_CANCEL_BILL,
    ID_BTN_PRINT,
    ID_BTN_CLEAR,
    ID_TXT_SEARCH,
    ID_TXT_BARCODE,
    ID_TXT_CUSTOMER,
    ID_TXT_DISCOUNT,
    ID_TIMER_SYNC_POLL,
};

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& db_path,
              const std::string& device_id,
              const PrintConfig& print_cfg);
    ~MainFrame();

private:
    // ── Worker thread ─────────────────────────────────────────────────────────
    PharmacyWorkerThread* m_worker;
    long long             m_next_request_id;
    long long             m_pending_checkout_req; // request_id awaiting checkout result

    // ── UI controls ──────────────────────────────────────────────────────────
    wxPanel*            m_panel;        // set in BuildLayout(); used by BuildGrid()
    wxTextCtrl*         m_txt_search;
    wxTextCtrl*         m_txt_barcode;
    wxTextCtrl*         m_txt_customer;
    wxTextCtrl*         m_txt_discount;
    wxGrid*             m_grid;
    BillingTableModel*  m_model;
    wxStaticText*       m_lbl_cgst;
    wxStaticText*       m_lbl_sgst;
    wxStaticText*       m_lbl_grand_total;
    wxButton*           m_btn_checkout;
    wxButton*           m_btn_clear;
    wxTimer             m_sync_timer;

    // ── Printing ──────────────────────────────────────────────────────────────
    PrinterManager*     m_printer;
    PrintConfig         m_print_cfg;

    // ── State ─────────────────────────────────────────────────────────────────
    std::string         m_device_id;
    std::string         m_last_bill_number;
    bool                m_checkout_in_progress;

    // Pending product fields — filled by ApplySearchResult, consumed by ApplyBatchResult
    long long           m_pending_batch_req;
    int                 m_pending_product_id;
    std::string         m_pending_product_name;
    std::string         m_pending_hsn_code;
    double              m_pending_gst_rate;
    double              m_pending_mrp;

    // ── Builders ─────────────────────────────────────────────────────────────
    void BuildLayout();
    void BuildGrid();
    void BuildToolbar();

    // ── Helpers ──────────────────────────────────────────────────────────────
    void RefreshTotals();
    long long NextRequestId() { return ++m_next_request_id; }
    void SetBusyState(bool busy);
    void ShowError(const wxString& msg);

    void ApplySearchResult (const std::string& json);
    void ApplyBatchResult  (const std::string& json);
    void ApplyCheckoutResult(const std::string& json);

    // ── Event handlers ────────────────────────────────────────────────────────
    void OnSearch       (wxCommandEvent&);
    void OnBarcodeEnter (wxCommandEvent&);
    void OnCheckout     (wxCommandEvent&);
    void OnCancelBill   (wxCommandEvent&);
    void OnPrint        (wxCommandEvent&);
    void OnClear        (wxCommandEvent&);
    void OnGridCellChange(wxGridEvent&);
    void OnSyncTimer    (wxTimerEvent&);
    void OnClose        (wxCloseEvent&);

    // Handlers for worker-thread events
    void OnDbResult (wxCommandEvent&);
    void OnDbError  (wxCommandEvent&);

    wxDECLARE_EVENT_TABLE();
};
