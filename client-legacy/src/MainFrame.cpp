// =============================================================================
// MainFrame.cpp  —  Main POS window: layout, event handling, GUI↔worker bridge
// =============================================================================

#include "MainFrame.h"
#include "WorkerThread.h"
#include "BillingGrid.h"
#include "GstEngine.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>
#include <cstdio>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON  (ID_BTN_SEARCH,     MainFrame::OnSearch)
    EVT_BUTTON  (ID_BTN_CHECKOUT,   MainFrame::OnCheckout)
    EVT_BUTTON  (ID_BTN_CANCEL_BILL,MainFrame::OnCancelBill)
    EVT_BUTTON  (ID_BTN_PRINT,      MainFrame::OnPrint)
    EVT_BUTTON  (ID_BTN_CLEAR,      MainFrame::OnClear)
    EVT_TEXT_ENTER(ID_TXT_SEARCH,   MainFrame::OnSearch)
    EVT_TEXT_ENTER(ID_TXT_BARCODE,  MainFrame::OnBarcodeEnter)
    EVT_GRID_CELL_CHANGED           (MainFrame::OnGridCellChange)
    EVT_TIMER   (ID_TIMER_SYNC_POLL,MainFrame::OnSyncTimer)
    EVT_CLOSE                       (MainFrame::OnClose)
    EVT_COMMAND (wxID_ANY, wxEVT_DB_RESULT, MainFrame::OnDbResult)
    EVT_COMMAND (wxID_ANY, wxEVT_DB_ERROR,  MainFrame::OnDbError)
wxEND_EVENT_TABLE()

// ─────────────────────────────────────────────────────────────────────────────

MainFrame::MainFrame(const wxString& db_path,
                     const std::string& device_id,
                     const PrintConfig& print_cfg)
    : wxFrame(nullptr, wxID_ANY, "Aushadhalayam — Pharmacy POS",
              wxDefaultPosition, wxSize(1280, 800))
    , m_worker(nullptr)
    , m_panel(nullptr)
    , m_next_request_id(0)
    , m_pending_checkout_req(-1)
    , m_model(nullptr)
    , m_sync_timer(this, ID_TIMER_SYNC_POLL)
    , m_print_cfg(print_cfg)
    , m_device_id(device_id)
    , m_checkout_in_progress(false)
    , m_pending_batch_req(-1)
    , m_pending_product_id(0)
    , m_pending_gst_rate(0.0)
    , m_pending_mrp(0.0)
{
    m_printer = new PrinterManager(print_cfg);

    BuildLayout();

    // ── Start worker thread ──────────────────────────────────────────────────
    m_worker = new PharmacyWorkerThread(this, db_path, device_id);
    if (m_worker->Create() != wxTHREAD_NO_ERROR) {
        wxMessageBox("Failed to create database thread", "Fatal Error",
                     wxICON_ERROR | wxOK);
        Close(true);
        return;
    }
    m_worker->Run();

    // Poll sync queue every 30 seconds for status display
    m_sync_timer.Start(30000);

    SetMinSize(wxSize(1024, 600));
    Centre();
    Show(true);
}

MainFrame::~MainFrame() {
    delete m_printer;
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout builders
// ─────────────────────────────────────────────────────────────────────────────

void MainFrame::BuildLayout() {
    // Create status bar
    CreateStatusBar(3);
    SetStatusText("Ready", 0);
    SetStatusText("Device: " + wxString::FromUTF8(m_device_id.substr(0,8).c_str()), 1);
    SetStatusText("Sync: --", 2);

    m_panel = new wxPanel(this);
    wxPanel* panel = m_panel;
    wxBoxSizer* vsizer = new wxBoxSizer(wxVERTICAL);

    // ── Top row: search + summary ────────────────────────────────────────────
    wxBoxSizer* top_row = new wxBoxSizer(wxHORIZONTAL);

    // Search panel
    wxStaticBoxSizer* search_box = new wxStaticBoxSizer(wxVERTICAL, panel, "Product Lookup");
    {
        wxBoxSizer* row1 = new wxBoxSizer(wxHORIZONTAL);
        row1->Add(new wxStaticText(panel, wxID_ANY, "Search:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 4);
        m_txt_search = new wxTextCtrl(panel, ID_TXT_SEARCH, "", wxDefaultPosition,
                                      wxSize(220,-1), wxTE_PROCESS_ENTER);
        row1->Add(m_txt_search, 1, wxALIGN_CENTER_VERTICAL);
        row1->Add(new wxButton(panel, ID_BTN_SEARCH, "Find"), 0, wxLEFT, 4);
        search_box->Add(row1, 0, wxEXPAND|wxALL, 4);

        wxBoxSizer* row2 = new wxBoxSizer(wxHORIZONTAL);
        row2->Add(new wxStaticText(panel, wxID_ANY, "Barcode:"), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 4);
        m_txt_barcode = new wxTextCtrl(panel, ID_TXT_BARCODE, "", wxDefaultPosition,
                                       wxSize(220,-1), wxTE_PROCESS_ENTER);
        row2->Add(m_txt_barcode, 1, wxALIGN_CENTER_VERTICAL);
        search_box->Add(row2, 0, wxEXPAND|wxALL, 4);
    }
    top_row->Add(search_box, 1, wxEXPAND|wxALL, 4);

    // Summary panel
    wxStaticBoxSizer* summary_box = new wxStaticBoxSizer(wxVERTICAL, panel, "Bill Summary");
    {
        wxFlexGridSizer* fg = new wxFlexGridSizer(2, wxSize(8,4));
        fg->Add(new wxStaticText(panel, wxID_ANY, "Patient:"), 0, wxALIGN_CENTER_VERTICAL);
        m_txt_customer = new wxTextCtrl(panel, ID_TXT_CUSTOMER, "", wxDefaultPosition, wxSize(180,-1));
        fg->Add(m_txt_customer, 0, wxEXPAND);
        fg->Add(new wxStaticText(panel, wxID_ANY, "Bill Disc %:"), 0, wxALIGN_CENTER_VERTICAL);
        m_txt_discount = new wxTextCtrl(panel, ID_TXT_DISCOUNT, "0");
        fg->Add(m_txt_discount, 0);
        summary_box->Add(fg, 0, wxALL, 4);

        wxFlexGridSizer* totals = new wxFlexGridSizer(2, wxSize(8,2));
        totals->Add(new wxStaticText(panel, wxID_ANY, "CGST:"));
        m_lbl_cgst        = new wxStaticText(panel, wxID_ANY, "₹0.00");
        totals->Add(m_lbl_cgst);
        totals->Add(new wxStaticText(panel, wxID_ANY, "SGST:"));
        m_lbl_sgst        = new wxStaticText(panel, wxID_ANY, "₹0.00");
        totals->Add(m_lbl_sgst);
        totals->Add(new wxStaticText(panel, wxID_ANY, "Grand Total:"));
        m_lbl_grand_total = new wxStaticText(panel, wxID_ANY, "₹0.00",
                                             wxDefaultPosition, wxDefaultSize,
                                             wxALIGN_RIGHT);
        wxFont bold = m_lbl_grand_total->GetFont();
        bold.MakeBold(); bold.SetPointSize(14);
        m_lbl_grand_total->SetFont(bold);
        totals->Add(m_lbl_grand_total);
        summary_box->Add(totals, 0, wxALL, 4);
    }
    top_row->Add(summary_box, 0, wxEXPAND|wxALL, 4);
    vsizer->Add(top_row, 0, wxEXPAND);

    // ── Grid ─────────────────────────────────────────────────────────────────
    BuildGrid();
    vsizer->Add(m_grid, 1, wxEXPAND|wxALL, 4);

    // ── Action buttons ────────────────────────────────────────────────────────
    wxBoxSizer* btn_row = new wxBoxSizer(wxHORIZONTAL);
    btn_row->AddStretchSpacer();
    m_btn_clear = new wxButton(panel, ID_BTN_CLEAR, "Clear Bill");
    btn_row->Add(m_btn_clear, 0, wxALL, 4);
    btn_row->Add(new wxButton(panel, ID_BTN_PRINT, "Print"), 0, wxALL, 4);
    btn_row->Add(new wxButton(panel, ID_BTN_CANCEL_BILL, "Cancel Bill"), 0, wxALL, 4);
    m_btn_checkout = new wxButton(panel, ID_BTN_CHECKOUT, "CHECKOUT");
    wxFont f = m_btn_checkout->GetFont(); f.MakeBold(); m_btn_checkout->SetFont(f);
    m_btn_checkout->SetBackgroundColour(wxColour(46,125,50));
    m_btn_checkout->SetForegroundColour(*wxWHITE);
    btn_row->Add(m_btn_checkout, 0, wxALL, 4);
    vsizer->Add(btn_row, 0, wxEXPAND);

    panel->SetSizerAndFit(vsizer);
}

void MainFrame::BuildGrid() {
    m_grid = new wxGrid(m_panel, wxID_ANY);
    m_model = new BillingTableModel();
    // wxGrid takes ownership of the table model
    m_grid->SetTable(m_model, /*takeOwnership=*/true);

    // Column widths
    m_grid->SetColSize(BC_SERIAL,     35);
    m_grid->SetColSize(BC_MEDICINE,  200);
    m_grid->SetColSize(BC_BATCH,      80);
    m_grid->SetColSize(BC_EXPIRY,     60);
    m_grid->SetColSize(BC_QTY,        50);
    m_grid->SetColSize(BC_MRP,        70);
    m_grid->SetColSize(BC_DISC,       55);
    m_grid->SetColSize(BC_TAXABLE,    80);
    m_grid->SetColSize(BC_GST_RATE,   55);
    m_grid->SetColSize(BC_LINE_TOTAL, 80);

    m_grid->SetRowLabelSize(0);          // hide row numbers (we draw own serial)
    m_grid->EnableDragColSize(true);
    m_grid->EnableDragRowSize(false);
    m_grid->SetSelectionMode(wxGrid::wxGridSelectRows);
    m_grid->DisableCellEditControl();    // we handle SetValue in the model
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void MainFrame::RefreshTotals() {
    double cgst = 0, sgst = 0, total = 0;
    for (size_t i = 0; i < m_model->Count(); ++i) {
        const BillItemRow& it = m_model->GetItem(static_cast<int>(i));
        cgst  += it.cgst_amount;
        sgst  += it.sgst_amount;
        total += it.line_total;
    }
    // Apply bill-level discount
    double disc_pct = 0;
    m_txt_discount->GetValue().ToDouble(&disc_pct);
    double discount = gst::round2(total * disc_pct / 100.0);
    total -= discount;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "₹%.2f", cgst);  m_lbl_cgst->SetLabel(buf);
    std::snprintf(buf, sizeof(buf), "₹%.2f", sgst);  m_lbl_sgst->SetLabel(buf);
    std::snprintf(buf, sizeof(buf), "₹%.2f", total); m_lbl_grand_total->SetLabel(buf);
}

void MainFrame::SetBusyState(bool busy) {
    m_checkout_in_progress = busy;
    m_btn_checkout->Enable(!busy);
    m_btn_clear->Enable(!busy);
    SetStatusText(busy ? "Processing..." : "Ready", 0);
    if (busy) wxBeginBusyCursor();
    else      wxEndBusyCursor();
}

void MainFrame::ShowError(const wxString& msg) {
    wxMessageBox(msg, "Error", wxICON_ERROR | wxOK, this);
}

// ─────────────────────────────────────────────────────────────────────────────
// Event handlers
// ─────────────────────────────────────────────────────────────────────────────

void MainFrame::OnSearch(wxCommandEvent& /*e*/) {
    wxString q = m_txt_search->GetValue().Trim();
    if (q.IsEmpty()) return;
    DbTask t;
    t.type       = DbCommandType::CMD_SEARCH_PRODUCT;
    t.payload    = "{\"query\":" + std::string("\"") + std::string(q.mb_str()) + "\"}";
    t.request_id = NextRequestId();
    m_worker->PostTask(t);
}

void MainFrame::OnBarcodeEnter(wxCommandEvent& /*e*/) {
    // Treat barcode as SKU exact search
    wxString sku = m_txt_barcode->GetValue().Trim();
    if (sku.IsEmpty()) return;
    DbTask t;
    t.type       = DbCommandType::CMD_SEARCH_PRODUCT;
    t.payload    = "{\"query\":" + std::string("\"") + std::string(sku.mb_str()) + "\"}";
    t.request_id = NextRequestId();
    m_worker->PostTask(t);
    m_txt_barcode->Clear();
}

void MainFrame::OnCheckout(wxCommandEvent& /*e*/) {
    if (m_model->Count() == 0) {
        ShowError("Bill is empty. Add items before checkout.");
        return;
    }
    if (m_checkout_in_progress) return;

    // Build checkout JSON payload
    wxString customer  = m_txt_customer->GetValue();
    double   disc_pct  = 0;
    m_txt_discount->GetValue().ToDouble(&disc_pct);

    std::string items_json;
    for (size_t i = 0; i < m_model->Count(); ++i) {
        const BillItemRow& it = m_model->GetItem(static_cast<int>(i));
        if (!items_json.empty()) items_json += ",";
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "{\"product_id\":%d,\"batch_id\":%d,"
            "\"product_name\":\"%s\",\"hsn_code\":\"%s\","
            "\"batch_number\":\"%s\",\"expiry_date\":\"%s\","
            "\"quantity\":%d,\"mrp\":%.2f,"
            "\"discount_pct\":%.2f,\"gst_rate\":%.1f}",
            it.product_id, it.batch_id,
            it.product_name.c_str(), it.hsn_code.c_str(),
            it.batch_number.c_str(), it.expiry_date.c_str(),
            it.quantity, it.mrp, it.discount_pct, it.gst_rate);
        items_json += buf;
    }

    char payload[4096];
    std::snprintf(payload, sizeof(payload),
        "{\"customer_name\":\"%s\",\"payment_mode\":\"CASH\","
        "\"discount_pct\":%.2f,\"is_interstate\":0,"
        "\"items\":[%s]}",
        std::string(customer.mb_str()).c_str(),
        disc_pct, items_json.c_str());

    DbTask t;
    t.type             = DbCommandType::CMD_CHECKOUT;
    t.payload          = payload;
    t.request_id       = NextRequestId();
    m_pending_checkout_req = t.request_id;
    m_worker->PostTask(t);
    SetBusyState(true);
}

void MainFrame::OnCancelBill(wxCommandEvent& /*e*/) {
    if (m_last_bill_number.empty()) {
        ShowError("No bill to cancel.");
        return;
    }
    int ans = wxMessageBox("Cancel bill " + wxString::FromUTF8(m_last_bill_number.c_str()) + "?",
                           "Confirm Cancel", wxYES_NO | wxICON_QUESTION, this);
    if (ans != wxYES) return;
    // In production: look up bill_id from last checkout result and post CMD_CANCEL_BILL
}

void MainFrame::OnPrint(wxCommandEvent& /*e*/) {
    SetStatusText("Sending to printer...", 0);
    // Build ReceiptData from model and call m_printer->PrintReceipt(...)
    // (ReceiptData population omitted for brevity; wires to m_model->GetItem())
    SetStatusText("Print sent.", 0);
}

void MainFrame::OnClear(wxCommandEvent& /*e*/) {
    m_grid->BeginBatch();   // suspend all repaints
    m_model->Clear();
    m_grid->EndBatch();
    RefreshTotals();
    m_txt_customer->Clear();
    m_txt_discount->SetValue("0");
}

void MainFrame::OnGridCellChange(wxGridEvent& e) {
    RefreshTotals();
    e.Skip();
}

void MainFrame::OnSyncTimer(wxTimerEvent& /*e*/) {
    DbTask t;
    t.type       = DbCommandType::CMD_FLUSH_SYNC;
    t.payload    = "{}";
    t.request_id = NextRequestId();
    m_worker->PostTask(t);
}

void MainFrame::OnClose(wxCloseEvent& e) {
    m_sync_timer.Stop();

    if (m_worker) {
        DbTask q;
        q.type       = DbCommandType::CMD_QUIT;
        q.request_id = -1;
        m_worker->PostTask(q);
        m_worker->Wait();   // block until Entry() exits cleanly
        // wxThread::Wait() does NOT delete the object on joinable threads
        delete m_worker;
        m_worker = nullptr;
    }
    e.Skip();
}

// ─────────────────────────────────────────────────────────────────────────────
// Worker thread event handlers (always called on the GUI thread)
// ─────────────────────────────────────────────────────────────────────────────

void MainFrame::OnDbResult(wxCommandEvent& e) {
    long long req_id = static_cast<long long>(e.GetExtraLong());
    std::string json  = std::string(e.GetString().mb_str());

    if (req_id == m_pending_checkout_req) {
        SetBusyState(false);
        m_pending_checkout_req = -1;
        ApplyCheckoutResult(json);
        return;
    }

    if (req_id == m_pending_batch_req) {
        m_pending_batch_req = -1;
        ApplyBatchResult(json);
        return;
    }

    // Search result array — queue a batch lookup for the first product
    if (!json.empty() && json[0] == '[' && json.size() > 2) {
        ApplySearchResult(json);
        return;
    }

    // Sync status update
    std::string key = "pending_sync_count";
    if (json.find(key) != std::string::npos) {
        std::string cnt = jsonutil::extract_str(json, key);
        SetStatusText("Sync pending: " + wxString::FromUTF8(cnt.c_str()), 2);
    }
}

void MainFrame::OnDbError(wxCommandEvent& e) {
    long long req_id = static_cast<long long>(e.GetExtraLong());
    if (req_id == m_pending_checkout_req) {
        SetBusyState(false);
        m_pending_checkout_req = -1;
    }
    ShowError(e.GetString());
    SetStatusText("Error", 0);
}

void MainFrame::ApplySearchResult(const std::string& json) {
    // json is an array; extract the first product object
    size_t obj_start = json.find('{');
    size_t obj_end   = json.find('}', obj_start);
    if (obj_start == std::string::npos || obj_end == std::string::npos) {
        SetStatusText("No product found.", 0);
        return;
    }
    std::string first = json.substr(obj_start, obj_end - obj_start + 1);

    // Cache the product fields so we can fill a BillItemRow once we have a batch
    m_pending_product_id   = static_cast<int>(jsonutil::extract_int(first, "id"));
    m_pending_product_name = jsonutil::extract_str(first, "name");
    m_pending_hsn_code     = jsonutil::extract_str(first, "hsn_code");
    m_pending_gst_rate     = jsonutil::extract_dbl(first, "gst_rate");
    m_pending_mrp          = jsonutil::extract_dbl(first, "mrp");

    // Ask the worker for FEFO stock batches for this product
    DbTask t;
    t.type       = DbCommandType::CMD_GET_STOCK_BATCH;
    char payload[64];
    std::snprintf(payload, sizeof(payload), "{\"product_id\":%d}", m_pending_product_id);
    t.payload    = payload;
    t.request_id = NextRequestId();
    m_pending_batch_req = t.request_id;
    m_worker->PostTask(t);
    SetStatusText("Loading batches...", 0);
}

void MainFrame::ApplyBatchResult(const std::string& json) {
    // Take the first available batch (already FEFO-ordered by the query)
    size_t obj_start = json.find('{');
    size_t obj_end   = json.find('}', obj_start);
    if (obj_start == std::string::npos || obj_end == std::string::npos) {
        SetStatusText("No stock available.", 0);
        return;
    }
    std::string first = json.substr(obj_start, obj_end - obj_start + 1);

    BillItemRow row;
    row.product_id    = m_pending_product_id;
    row.product_name  = m_pending_product_name;
    row.hsn_code      = m_pending_hsn_code;
    row.gst_rate      = m_pending_gst_rate;
    row.mrp           = m_pending_mrp;
    row.batch_id      = static_cast<int>(jsonutil::extract_int(first, "id"));
    row.batch_number  = jsonutil::extract_str(first, "batch_number");
    row.expiry_date   = jsonutil::extract_str(first, "expiry_date");
    row.quantity      = 1;
    row.discount_pct  = 0.0;

    m_grid->BeginBatch();
    m_model->AppendItem(row);
    m_grid->EndBatch();
    m_grid->MakeCellVisible(static_cast<int>(m_model->Count()) - 1, BC_QTY);
    RefreshTotals();
    SetStatusText("Item added — edit Qty or Disc% in the grid.", 0);
}

void MainFrame::ApplyCheckoutResult(const std::string& json) {
    std::string bill_no    = jsonutil::extract_str(json, "bill_number");
    std::string grand_total= jsonutil::extract_str(json, "grand_total");
    m_last_bill_number     = bill_no;

    wxMessageBox("Bill " + wxString::FromUTF8(bill_no.c_str()) +
                 " saved.\nGrand Total: ₹" +
                 wxString::FromUTF8(grand_total.c_str()),
                 "Checkout Complete", wxICON_INFORMATION | wxOK, this);

    // Clear the bill for the next customer
    m_grid->BeginBatch();
    m_model->Clear();
    m_grid->EndBatch();
    RefreshTotals();
    m_txt_customer->Clear();
    SetStatusText("Checkout complete. Ready.", 0);

    // Auto-print if configured
    if (m_print_cfg.mode == PrintMode::GDI_SPOOLER ||
        m_print_cfg.mode == PrintMode::ESCPOS_COM) {
        wxCommandEvent dummy;
        OnPrint(dummy);
    }
}
