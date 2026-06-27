// =============================================================================
// BillingPage.cpp
// =============================================================================

#include "BillingPage.h"
#include "AppColors.h"
#include "GstEngine.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/msgdlg.h>
#include <wx/font.h>
#include <cstdio>
#include <cstring>
#include <sstream>

wxDEFINE_EVENT(wxEVT_BILL_COMPLETE, wxCommandEvent);

const wxString BillingPage::kPayModes[] = {
    "CASH", "UPI", "CARD", "CREDIT", "CHEQUE"
};

wxBEGIN_EVENT_TABLE(BillingPage, BasePage)
    EVT_BUTTON(BP_BTN_SEARCH,      BillingPage::OnSearch)
    EVT_TEXT_ENTER(BP_TXT_SEARCH,  BillingPage::OnSearch)
    EVT_TEXT_ENTER(BP_TXT_BARCODE, BillingPage::OnBarcodeEnter)
    EVT_BUTTON(BP_BTN_CHECKOUT,    BillingPage::OnCheckout)
    EVT_BUTTON(BP_BTN_CLEAR,       BillingPage::OnClear)
    EVT_GRID_CELL_CHANGED          (BillingPage::OnGridCellChange)
    EVT_TEXT(BP_TXT_CUSTOMER,      BillingPage::OnCustText)
    EVT_TIMER(BP_TIMER_CUST,       BillingPage::OnCustTimer)
    EVT_LISTBOX(wxID_ANY,          BillingPage::OnCustSelect)
wxEND_EVENT_TABLE()

BillingPage::BillingPage(wxWindow* parent, PharmacyWorkerThread* worker,
                         const std::string& device_id)
    : BasePage(parent, worker)
    , m_device_id(device_id)
    , m_cust_timer(this, BP_TIMER_CUST)
    , m_req_cust(-1), m_req_search(-1), m_req_batch(-1), m_req_checkout(-1)
    , m_pending_product_id(0), m_pending_gst_rate(0), m_pending_mrp(0)
    , m_pending_is_scheduled(0), m_checkout_busy(false)
{
    BuildUI();
}

void BillingPage::OnPageShown() {}

// ── Layout ────────────────────────────────────────────────────────────────────

void BillingPage::BuildUI() {
    wxBoxSizer* root = new wxBoxSizer(wxHORIZONTAL);

    // ── Left: search + grid ─────────────────────────────────────────────────
    wxBoxSizer* left = new wxBoxSizer(wxVERTICAL);
    left->AddSpacer(sz::CardPad);

    // Search row
    {
        wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
        m_txt_search = new wxTextCtrl(this, BP_TXT_SEARCH, "",
            wxDefaultPosition, wxSize(240, sz::InputH), wxTE_PROCESS_ENTER);
        m_txt_search->SetHint("Search medicine…");
        row->Add(m_txt_search, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, sz::RowGap);

        wxButton* btn = new wxButton(this, BP_BTN_SEARCH, "Find",
            wxDefaultPosition, wxSize(-1, sz::BtnH));
        btn->SetBackgroundColour(clr::Brand());
        btn->SetForegroundColour(*wxWHITE);
        row->Add(btn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, sz::RowGap);

        m_txt_barcode = new wxTextCtrl(this, BP_TXT_BARCODE, "",
            wxDefaultPosition, wxSize(140, sz::InputH), wxTE_PROCESS_ENTER);
        m_txt_barcode->SetHint("Barcode scan…");
        row->Add(m_txt_barcode, 0, wxALIGN_CENTER_VERTICAL);
        left->Add(row, 0, wxLEFT | wxRIGHT, sz::CardPad);
    }

    left->AddSpacer(sz::RowGap);

    // Grid
    BuildGrid(this, left);

    // Spacer at bottom
    left->AddSpacer(sz::CardPad);

    root->Add(left, 3, wxEXPAND);

    // ── Right: panel ────────────────────────────────────────────────────────
    wxPanel* right_panel = new wxPanel(this);
    right_panel->SetBackgroundColour(clr::Bg());
    right_panel->SetMinSize(wxSize(300, -1));
    right_panel->SetMaxSize(wxSize(320, -1));

    wxBoxSizer* right = new wxBoxSizer(wxVERTICAL);
    right->AddSpacer(sz::CardPad);

    // ── Customer card ────────────────────────────────────────────────────────
    {
        wxPanel* card = new wxPanel(right_panel);
        card->SetBackgroundColour(clr::Surface());
        wxBoxSizer* cv = new wxBoxSizer(wxVERTICAL);
        cv->AddSpacer(sz::CardPad);
        wxFont lf; lf.SetPointSize(sz::FontPt); lf.MakeBold();
        wxStaticText* t = new wxStaticText(card, wxID_ANY, "Customer");
        t->SetFont(lf); t->SetForegroundColour(clr::Text());
        cv->Add(t, 0, wxLEFT | wxBOTTOM, sz::CardPad);
        // Typeahead field
        m_txt_customer = new wxTextCtrl(card, BP_TXT_CUSTOMER, "",
            wxDefaultPosition, wxSize(-1, sz::InputH));
        m_txt_customer->SetHint("Name or phone (optional)");
        cv->Add(m_txt_customer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);
        card->SetSizer(cv);
        right->Add(card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::RowGap);

        // Popup listbox — positioned in OnSize or lazily placed
        m_cust_popup = new wxListBox(right_panel, wxID_ANY, wxDefaultPosition,
                                     wxSize(280, 120), 0, nullptr, wxLB_SINGLE);
        m_cust_popup->Hide();
        m_cust_popup->SetBackgroundColour(clr::Surface());
        m_cust_popup->SetForegroundColour(clr::Text());
    }

    // ── Doctor / RX card (hidden by default) ─────────────────────────────────
    {
        m_rx_panel = new wxPanel(right_panel);
        m_rx_panel->SetBackgroundColour(clr::WarningBg());
        wxBoxSizer* cv = new wxBoxSizer(wxVERTICAL);
        cv->AddSpacer(sz::CardPad);

        m_lbl_rx_warn = new wxStaticText(m_rx_panel, wxID_ANY,
            "Prescription required  [Sch H / H1 / X]");
        wxFont wf; wf.SetPointSize(sz::FontPt); wf.MakeBold();
        m_lbl_rx_warn->SetFont(wf);
        m_lbl_rx_warn->SetForegroundColour(clr::Warning());
        cv->Add(m_lbl_rx_warn, 0, wxLEFT | wxBOTTOM, sz::CardPad);

        wxFont lf; lf.SetPointSize(9);
        wxStaticText* dl = new wxStaticText(m_rx_panel, wxID_ANY, "Doctor Name *");
        dl->SetFont(lf); dl->SetForegroundColour(clr::Text2());
        cv->Add(dl, 0, wxLEFT, sz::CardPad);
        m_txt_doctor = new wxTextCtrl(m_rx_panel, BP_TXT_DOCTOR, "",
            wxDefaultPosition, wxSize(-1, sz::InputH));
        cv->Add(m_txt_doctor, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);

        wxStaticText* rl = new wxStaticText(m_rx_panel, wxID_ANY, "Prescription No.");
        rl->SetFont(lf); rl->SetForegroundColour(clr::Text2());
        cv->Add(rl, 0, wxLEFT, sz::CardPad);
        m_txt_rx_no = new wxTextCtrl(m_rx_panel, BP_TXT_RX_NO, "",
            wxDefaultPosition, wxSize(-1, sz::InputH));
        cv->Add(m_txt_rx_no, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);

        m_rx_panel->SetSizer(cv);
        m_rx_panel->Hide();
        right->Add(m_rx_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::RowGap);
    }

    // ── Notes ────────────────────────────────────────────────────────────────
    {
        wxPanel* card = new wxPanel(right_panel);
        card->SetBackgroundColour(clr::Surface());
        wxBoxSizer* cv = new wxBoxSizer(wxVERTICAL);
        cv->AddSpacer(6);
        wxFont lf; lf.SetPointSize(9);
        wxStaticText* nl = new wxStaticText(card, wxID_ANY, "Notes");
        nl->SetFont(lf); nl->SetForegroundColour(clr::Text2());
        cv->Add(nl, 0, wxLEFT, sz::CardPad);
        m_txt_notes = new wxTextCtrl(card, BP_TXT_NOTES, "",
            wxDefaultPosition, wxSize(-1, 50), wxTE_MULTILINE);
        cv->Add(m_txt_notes, 0, wxEXPAND | wxALL, sz::CardPad);
        card->SetSizer(cv);
        right->Add(card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::RowGap);
    }

    // ── Totals ────────────────────────────────────────────────────────────────
    {
        wxPanel* card = new wxPanel(right_panel);
        card->SetBackgroundColour(clr::Surface());
        wxFlexGridSizer* fg = new wxFlexGridSizer(2, wxSize(sz::RowGap, 4));
        fg->AddGrowableCol(1);

        wxFont lf; lf.SetPointSize(sz::FontPt);
        auto addRow = [&](const wxString& label, wxStaticText*& out) {
            wxStaticText* k = new wxStaticText(card, wxID_ANY, label);
            k->SetFont(lf); k->SetForegroundColour(clr::Text2());
            fg->Add(k, 0, wxALIGN_CENTER_VERTICAL);
            out = new wxStaticText(card, wxID_ANY, "Rs 0.00",
                wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
            out->SetFont(lf); out->SetForegroundColour(clr::Text());
            fg->Add(out, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
        };

        addRow("CGST:", m_lbl_cgst);
        addRow("SGST:", m_lbl_sgst);

        // Bill disc
        wxStaticText* dk = new wxStaticText(card, wxID_ANY, "Bill Disc %:");
        dk->SetFont(lf); dk->SetForegroundColour(clr::Text2());
        fg->Add(dk, 0, wxALIGN_CENTER_VERTICAL);
        m_txt_disc = new wxTextCtrl(card, wxID_ANY, "0",
            wxDefaultPosition, wxSize(60, sz::InputH));
        fg->Add(m_txt_disc, 0, wxALIGN_CENTER_VERTICAL);
        m_txt_disc->Bind(wxEVT_TEXT, [this](wxCommandEvent&){ RefreshTotals(); });

        // Grand total — larger font
        wxStaticText* gk = new wxStaticText(card, wxID_ANY, "Grand Total:");
        wxFont gf; gf.SetPointSize(12); gf.MakeBold();
        gk->SetFont(gf); gk->SetForegroundColour(clr::Text());
        fg->Add(gk, 0, wxALIGN_CENTER_VERTICAL | wxTOP, 4);
        m_lbl_total = new wxStaticText(card, wxID_ANY, "Rs 0.00",
            wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
        m_lbl_total->SetFont(gf); m_lbl_total->SetForegroundColour(clr::Brand());
        fg->Add(m_lbl_total, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP, 4);

        wxBoxSizer* cv = new wxBoxSizer(wxVERTICAL);
        cv->Add(fg, 0, wxEXPAND | wxALL, sz::CardPad);
        card->SetSizer(cv);
        right->Add(card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::RowGap);
    }

    // ── Payment mode ──────────────────────────────────────────────────────────
    {
        wxArrayString payArr;
        for (const auto& s : kPayModes) payArr.Add(s);
        m_radio_pay = new wxRadioBox(right_panel, wxID_ANY, "Payment Mode",
            wxDefaultPosition, wxDefaultSize, payArr, 5, wxRA_SPECIFY_COLS);
        right->Add(m_radio_pay, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::RowGap);
    }

    // ── Action buttons ────────────────────────────────────────────────────────
    {
        m_btn_checkout = new wxButton(right_panel, BP_BTN_CHECKOUT, "CHECKOUT",
            wxDefaultPosition, wxSize(-1, sz::BtnHLg));
        wxFont bf; bf.SetPointSize(sz::FontPt); bf.MakeBold();
        m_btn_checkout->SetFont(bf);
        m_btn_checkout->SetBackgroundColour(clr::Success());
        m_btn_checkout->SetForegroundColour(*wxWHITE);
        right->Add(m_btn_checkout, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::RowGap);

        m_btn_clear = new wxButton(right_panel, BP_BTN_CLEAR, "Clear Bill",
            wxDefaultPosition, wxSize(-1, sz::BtnH));
        right->Add(m_btn_clear, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::RowGap);
    }

    right_panel->SetSizer(right);
    root->Add(right_panel, 0, wxEXPAND | wxLEFT, sz::RowGap);

    SetSizer(root);
}

void BillingPage::BuildGrid(wxPanel*, wxBoxSizer* sizer) {
    m_grid = new wxGrid(this, wxID_ANY);
    m_model = new BillingTableModel();
    m_grid->SetTable(m_model, true);

    // Style
    m_grid->SetBackgroundColour(clr::Surface());
    m_grid->SetDefaultCellBackgroundColour(clr::Surface());
    m_grid->SetDefaultCellTextColour(clr::Text());
    m_grid->SetLabelBackgroundColour(clr::SurfaceAlt());
    m_grid->SetLabelTextColour(clr::Text2());
    m_grid->SetGridLineColour(clr::Border());
    m_grid->EnableDragRowSize(false);
    m_grid->SetSelectionMode(wxGrid::wxGridSelectRows);
    m_grid->SetRowLabelSize(0);
    m_grid->EnableDragColSize(true);

    wxFont hf; hf.SetPointSize(9); hf.MakeBold();
    m_grid->SetLabelFont(hf);

    // Column widths
    m_grid->SetColSize(BC_SERIAL,     28);
    m_grid->SetColSize(BC_MEDICINE,  200);
    m_grid->SetColSize(BC_BATCH,     140);
    m_grid->SetColSize(BC_EXPIRY,     52);
    m_grid->SetColSize(BC_QTY,        44);
    m_grid->SetColSize(BC_MRP,        64);
    m_grid->SetColSize(BC_DISC,       50);
    m_grid->SetColSize(BC_TAXABLE,    70);
    m_grid->SetColSize(BC_GST_RATE,   44);
    m_grid->SetColSize(BC_LINE_TOTAL, 72);

    // Default row height to match React table rows (compact)
    m_grid->SetDefaultRowSize(24, true);

    sizer->Add(m_grid, 1, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void BillingPage::RefreshTotals() {
    double cgst = 0, sgst = 0, total = 0;
    for (size_t i = 0; i < m_model->Count(); ++i) {
        const BillItemRow& it = m_model->GetItem((int)i);
        cgst  += it.cgst_amount;
        sgst  += it.sgst_amount;
        total += it.line_total;
    }
    double disc_pct = 0;
    m_txt_disc->GetValue().ToDouble(&disc_pct);
    double discount = gst::round2(total * disc_pct / 100.0);
    total -= discount;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "Rs %.2f", cgst);  m_lbl_cgst->SetLabel(buf);
    std::snprintf(buf, sizeof(buf), "Rs %.2f", sgst);  m_lbl_sgst->SetLabel(buf);
    std::snprintf(buf, sizeof(buf), "Rs %.2f", total); m_lbl_total->SetLabel(buf);

    UpdateRxVisibility();
    Layout();
}

void BillingPage::UpdateRxVisibility() {
    bool needs_rx = m_model->HasScheduledItem();
    if (m_rx_panel->IsShown() != needs_rx) {
        m_rx_panel->Show(needs_rx);
        GetParent()->Layout();
        Layout();
    }
}

void BillingPage::HideCustPopup() {
    if (m_cust_popup->IsShown()) {
        m_cust_popup->Hide();
        Layout();
    }
}

void BillingPage::ShowCustPopup() {
    if (m_cust_results.empty()) { HideCustPopup(); return; }

    // Position popup below customer text field
    wxPoint pos = m_txt_customer->GetParent()->ClientToScreen(
        m_txt_customer->GetPosition());
    pos.y += m_txt_customer->GetSize().y;
    pos = ScreenToClient(pos);

    m_cust_popup->SetPosition(pos);
    m_cust_popup->SetSize(m_txt_customer->GetSize().x, 100);
    m_cust_popup->Show();
    m_cust_popup->Raise();
}

void BillingPage::PostCustomerSearch(const wxString& q) {
    DbTask t;
    t.type = DbCommandType::CMD_SEARCH_CUSTOMER;
    t.payload = "{\"query\":\"" + std::string(q.utf8_str()) + "\"}";
    t.request_id = next_req_id();
    m_req_cust = PostReq(t.request_id);
    m_worker->PostTask(t);
}

void BillingPage::PostSearchTask(const wxString& q) {
    DbTask t;
    t.type = DbCommandType::CMD_SEARCH_PRODUCT;
    t.payload = "{\"query\":\"" + std::string(q.utf8_str()) + "\"}";
    t.request_id = next_req_id();
    m_req_search = PostReq(t.request_id);
    m_worker->PostTask(t);
}

// ── Event handlers ────────────────────────────────────────────────────────────

void BillingPage::OnSearch(wxCommandEvent&) {
    wxString q = m_txt_search->GetValue().Trim();
    if (!q.IsEmpty()) PostSearchTask(q);
}

void BillingPage::OnBarcodeEnter(wxCommandEvent&) {
    wxString q = m_txt_barcode->GetValue().Trim();
    if (!q.IsEmpty()) { PostSearchTask(q); m_txt_barcode->Clear(); }
}

void BillingPage::OnCustText(wxCommandEvent&) {
    m_cust_timer.Stop();
    wxString v = m_txt_customer->GetValue().Trim();
    if (v.Length() < 2) { HideCustPopup(); return; }
    m_cust_timer.StartOnce(280);
}

void BillingPage::OnCustTimer(wxTimerEvent&) {
    PostCustomerSearch(m_txt_customer->GetValue().Trim());
}

void BillingPage::OnCustSelect(wxCommandEvent& e) {
    if (e.GetEventObject() != m_cust_popup) { e.Skip(); return; }
    int sel = m_cust_popup->GetSelection();
    if (sel < 0 || sel >= (int)m_cust_results.size()) return;
    m_txt_customer->SetValue(wxString::FromUTF8(m_cust_results[sel].name.c_str()));
    HideCustPopup();
}

void BillingPage::OnCheckout(wxCommandEvent&) {
    if (m_model->Count() == 0) {
        wxMessageBox("Bill is empty.", "Checkout", wxICON_WARNING | wxOK, this);
        return;
    }
    if (m_checkout_busy) return;
    if (m_model->HasScheduledItem() && m_txt_doctor->GetValue().Trim().IsEmpty()) {
        wxMessageBox("Doctor name is required for Schedule H/H1/X medicines.",
                     "Prescription Required", wxICON_WARNING | wxOK, this);
        m_txt_doctor->SetFocus();
        return;
    }

    wxString customer   = m_txt_customer->GetValue();
    wxString doctor     = m_txt_doctor->GetValue();
    wxString rx_no      = m_txt_rx_no->GetValue();
    wxString notes      = m_txt_notes->GetValue();
    wxString pay_mode   = kPayModes[m_radio_pay->GetSelection()];
    double   disc_pct   = 0;
    m_txt_disc->GetValue().ToDouble(&disc_pct);

    std::string items_json;
    for (size_t i = 0; i < m_model->Count(); ++i) {
        const BillItemRow& it = m_model->GetItem((int)i);
        if (!items_json.empty()) items_json += ",";
        char buf[1024];
        std::snprintf(buf, sizeof(buf),
            "{\"product_id\":%d,\"batch_id\":%d,"
            "\"product_name\":\"%s\",\"hsn_code\":\"%s\","
            "\"batch_number\":\"%s\",\"expiry_date\":\"%s\","
            "\"quantity\":%d,\"mrp\":%.2f,"
            "\"discount_pct\":%.2f,\"gst_rate\":%.1f}",
            it.product_id, it.batch_id,
            jsonutil::escape(it.product_name).c_str(),
            jsonutil::escape(it.hsn_code).c_str(),
            jsonutil::escape(it.batch_number).c_str(),
            jsonutil::escape(it.expiry_date).c_str(),
            it.quantity, it.mrp, it.discount_pct, it.gst_rate);
        items_json += buf;
    }

    char payload[8192];
    std::snprintf(payload, sizeof(payload),
        "{\"customer_name\":\"%s\",\"doctor_name\":\"%s\","
        "\"prescription_no\":\"%s\",\"notes\":\"%s\","
        "\"payment_mode\":\"%s\",\"discount_pct\":%.2f,"
        "\"is_interstate\":0,\"items\":[%s]}",
        jsonutil::escape(std::string(customer.utf8_str())).c_str(),
        jsonutil::escape(std::string(doctor.utf8_str())).c_str(),
        jsonutil::escape(std::string(rx_no.utf8_str())).c_str(),
        jsonutil::escape(std::string(notes.utf8_str())).c_str(),
        std::string(pay_mode.mb_str()).c_str(),
        disc_pct, items_json.c_str());

    DbTask t;
    t.type = DbCommandType::CMD_CHECKOUT;
    t.payload = payload;
    t.request_id = next_req_id();
    m_req_checkout = PostReq(t.request_id);
    m_worker->PostTask(t);

    m_checkout_busy = true;
    m_btn_checkout->Enable(false);
    m_btn_checkout->SetLabel("Processing…");
}

void BillingPage::OnClear(wxCommandEvent&) {
    m_grid->BeginBatch();
    m_model->Clear();
    m_grid->EndBatch();
    RefreshTotals();
    m_txt_customer->Clear();
    m_txt_doctor->Clear();
    m_txt_rx_no->Clear();
    m_txt_notes->Clear();
    m_txt_disc->SetValue("0");
    m_radio_pay->SetSelection(0);
}

void BillingPage::OnGridCellChange(wxGridEvent& e) {
    RefreshTotals();
    e.Skip();
}

// ── DB result dispatch ────────────────────────────────────────────────────────

void BillingPage::HandleDbResult(long long req_id, const std::string& json) {
    if (req_id == m_req_search)   { ApplySearchResult(json);  return; }
    if (req_id == m_req_batch)    { ApplyBatchResult(json);   return; }
    if (req_id == m_req_checkout) { ApplyCheckoutResult(json); return; }
    if (req_id == m_req_cust)     { ApplyCustResult(json);    return; }
}

void BillingPage::ApplySearchResult(const std::string& json) {
    size_t obj_s = json.find('{');
    size_t obj_e = json.find('}', obj_s);
    if (obj_s == std::string::npos || obj_e == std::string::npos) return;
    std::string first = json.substr(obj_s, obj_e - obj_s + 1);

    m_pending_product_id   = (int)jsonutil::extract_int(first, "id");
    m_pending_product_name = jsonutil::extract_str(first, "name");
    m_pending_hsn_code     = jsonutil::extract_str(first, "hsn_code");
    m_pending_gst_rate     = jsonutil::extract_dbl(first, "gst_rate");
    m_pending_mrp          = jsonutil::extract_dbl(first, "mrp");
    m_pending_is_scheduled = (int)jsonutil::extract_int(first, "is_scheduled");

    DbTask t;
    t.type = DbCommandType::CMD_GET_STOCK_BATCH;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "{\"product_id\":%d}", m_pending_product_id);
    t.payload = buf;
    t.request_id = next_req_id();
    m_req_batch = PostReq(t.request_id);
    m_worker->PostTask(t);
}

void BillingPage::ApplyBatchResult(const std::string& json) {
    // Build batch_options from the returned array
    std::vector<BatchOption> opts;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);
        BatchOption b;
        b.id           = (int)jsonutil::extract_int(obj, "id");
        b.batch_number = jsonutil::extract_str(obj, "batch_number");
        b.expiry_date  = jsonutil::extract_str(obj, "expiry_date");
        b.mrp          = jsonutil::extract_dbl(obj, "mrp");
        b.quantity     = (int)jsonutil::extract_int(obj, "quantity");
        opts.push_back(b);
        pos = end + 1;
    }

    if (opts.empty()) return;

    // Use first batch (FEFO) as default
    const BatchOption& first = opts[0];
    BillItemRow row;
    row.product_id    = m_pending_product_id;
    row.product_name  = m_pending_product_name;
    row.hsn_code      = m_pending_hsn_code;
    row.gst_rate      = m_pending_gst_rate;
    row.mrp           = first.mrp;
    row.batch_id      = first.id;
    row.batch_number  = first.batch_number;
    row.expiry_date   = first.expiry_date;
    row.quantity      = 1;
    row.discount_pct  = 0.0;
    row.is_scheduled  = m_pending_is_scheduled;
    row.batch_options = opts;

    m_grid->BeginBatch();
    m_model->AppendItem(row);
    m_grid->EndBatch();
    m_grid->MakeCellVisible((int)m_model->Count() - 1, BC_QTY);
    RefreshTotals();
    m_txt_search->Clear();
}

void BillingPage::ApplyCheckoutResult(const std::string& json) {
    m_checkout_busy = false;
    m_btn_checkout->Enable(true);
    m_btn_checkout->SetLabel("CHECKOUT");

    std::string bill_no    = jsonutil::extract_str(json, "bill_number");
    std::string grand_total= jsonutil::extract_str(json, "grand_total");
    m_last_bill_no = bill_no;

    wxMessageBox(
        "Bill " + wxString::FromUTF8(bill_no.c_str()) +
        " saved.\nTotal: Rs " + wxString::FromUTF8(grand_total.c_str()),
        "Checkout Complete", wxICON_INFORMATION | wxOK, this);

    m_grid->BeginBatch(); m_model->Clear(); m_grid->EndBatch();
    m_txt_customer->Clear(); m_txt_doctor->Clear();
    m_txt_rx_no->Clear(); m_txt_notes->Clear();
    m_txt_disc->SetValue("0");
    m_radio_pay->SetSelection(0);
    RefreshTotals();
}

void BillingPage::ApplyCustResult(const std::string& json) {
    m_cust_results.clear();
    m_cust_popup->Clear();

    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);
        CustEntry e;
        e.id    = jsonutil::extract_str(obj, "id");
        e.name  = jsonutil::extract_str(obj, "name");
        e.phone = jsonutil::extract_str(obj, "phone");
        m_cust_results.push_back(e);
        wxString display = wxString::FromUTF8(e.name.c_str()) +
            "  " + wxString::FromUTF8(e.phone.c_str());
        m_cust_popup->Append(display);
        pos = end + 1;
    }

    if (!m_cust_results.empty()) ShowCustPopup();
    else HideCustPopup();
}
