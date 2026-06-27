#pragma once
// =============================================================================
// BillingPage.h  —  Full billing UI panel
//
// Moved from MainFrame and enhanced with:
//   • Customer typeahead popup (wxTextCtrl + floating wxListBox)
//   • Doctor / Prescription fields (shown when Sch H/H1/X item added)
//   • Payment mode radio group (CASH/UPI/CARD/CREDIT/CHEQUE)
//   • Per-item batch selector via wxGridCellChoiceEditor
//   • Notes field
// =============================================================================

#include "Pages.h"
#include "BillingGrid.h"
#include "WorkerThread.h"

#include <wx/grid.h>
#include <wx/listbox.h>
#include <wx/radiobox.h>

// Event fired when checkout succeeds (carries bill number as string)
wxDECLARE_EVENT(wxEVT_BILL_COMPLETE, wxCommandEvent);

enum BillingPageIds {
    BP_TXT_SEARCH    = wxID_HIGHEST + 100,
    BP_BTN_SEARCH,
    BP_TXT_BARCODE,
    BP_TXT_CUSTOMER,
    BP_TXT_DOCTOR,
    BP_TXT_RX_NO,
    BP_TXT_NOTES,
    BP_TXT_DISC,
    BP_BTN_CHECKOUT,
    BP_BTN_CLEAR,
    BP_TIMER_CUST,
};

class BillingPage : public BasePage {
public:
    BillingPage(wxWindow* parent, PharmacyWorkerThread* worker,
                const std::string& device_id);
    void OnPageShown() override;

private:
    std::string m_device_id;

    // ── Billing grid ──────────────────────────────────────────────────────────
    wxGrid*            m_grid;
    BillingTableModel* m_model;

    // ── Search controls ───────────────────────────────────────────────────────
    wxTextCtrl* m_txt_search;
    wxTextCtrl* m_txt_barcode;

    // ── Customer typeahead ────────────────────────────────────────────────────
    wxTextCtrl* m_txt_customer;
    wxListBox*  m_cust_popup;
    wxTimer     m_cust_timer;
    struct CustEntry { std::string id, name, phone; };
    std::vector<CustEntry> m_cust_results;
    long long m_req_cust;

    // ── Doctor / prescription (shown when scheduled item present) ─────────────
    wxStaticText* m_lbl_rx_warn;
    wxTextCtrl*   m_txt_doctor;
    wxTextCtrl*   m_txt_rx_no;
    wxPanel*      m_rx_panel;

    // ── Notes ─────────────────────────────────────────────────────────────────
    wxTextCtrl*   m_txt_notes;

    // ── Payment modes ─────────────────────────────────────────────────────────
    wxRadioBox*   m_radio_pay;
    static const wxString kPayModes[];

    // ── Summary labels ────────────────────────────────────────────────────────
    wxTextCtrl*   m_txt_disc;
    wxStaticText* m_lbl_cgst;
    wxStaticText* m_lbl_sgst;
    wxStaticText* m_lbl_total;
    wxButton*     m_btn_checkout;
    wxButton*     m_btn_clear;

    // ── Pending DB state ──────────────────────────────────────────────────────
    long long m_req_search;
    long long m_req_batch;
    long long m_req_checkout;

    int         m_pending_product_id;
    std::string m_pending_product_name;
    std::string m_pending_hsn_code;
    double      m_pending_gst_rate;
    double      m_pending_mrp;
    int         m_pending_is_scheduled;

    bool m_checkout_busy;
    std::string m_last_bill_no;

    // ── Builder ───────────────────────────────────────────────────────────────
    void BuildUI();
    void BuildGrid(wxPanel* parent, wxBoxSizer* sizer);

    // ── Helpers ───────────────────────────────────────────────────────────────
    void RefreshTotals();
    void UpdateRxVisibility();
    void HideCustPopup();
    void ShowCustPopup();
    void PostSearchTask(const wxString& q);
    void PostCustomerSearch(const wxString& q);

    void HandleDbResult(long long req_id, const std::string& json) override;
    void ApplySearchResult (const std::string& json);
    void ApplyBatchResult  (const std::string& json);
    void ApplyCheckoutResult(const std::string& json);
    void ApplyCustResult   (const std::string& json);

    // ── Event handlers ────────────────────────────────────────────────────────
    void OnSearch       (wxCommandEvent&);
    void OnBarcodeEnter (wxCommandEvent&);
    void OnCheckout     (wxCommandEvent&);
    void OnClear        (wxCommandEvent&);
    void OnGridCellChange(wxGridEvent&);
    void OnCustText     (wxCommandEvent&);
    void OnCustTimer    (wxTimerEvent&);
    void OnCustSelect   (wxCommandEvent&);

    wxDECLARE_EVENT_TABLE();
};
