// =============================================================================
// DashboardPage.cpp
// =============================================================================

#include "DashboardPage.h"
#include "AppColors.h"
#include "WorkerThread.h"

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <cstdio>
#include <ctime>
#include <sstream>

static wxStaticText* MakeKpiCard(wxWindow* parent, wxBoxSizer* row,
                                  const wxString& label, wxColour iconBg)
{
    wxPanel* card = new wxPanel(parent, wxID_ANY);
    card->SetBackgroundColour(clr::Surface());
    card->SetMinSize(wxSize(160, 80));

    wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);

    // Coloured top stripe
    wxPanel* stripe = new wxPanel(card, wxID_ANY, wxDefaultPosition, wxSize(-1, 4));
    stripe->SetBackgroundColour(iconBg);
    vs->Add(stripe, 0, wxEXPAND);

    wxBoxSizer* inner = new wxBoxSizer(wxVERTICAL);
    inner->AddSpacer(8);

    wxFont lf; lf.SetPointSize(8); lf.MakeBold();
    wxStaticText* lbl = new wxStaticText(card, wxID_ANY, label);
    lbl->SetFont(lf);
    lbl->SetForegroundColour(clr::Text2());
    inner->Add(lbl, 0, wxLEFT, sz::CardPad);

    wxFont vf; vf.SetPointSize(16); vf.MakeBold();
    wxStaticText* val = new wxStaticText(card, wxID_ANY, "0");
    val->SetFont(vf);
    val->SetForegroundColour(clr::Text());
    inner->Add(val, 0, wxLEFT | wxTOP, sz::CardPad);
    inner->AddStretchSpacer();

    vs->Add(inner, 1, wxEXPAND);
    card->SetSizer(vs);

    row->Add(card, 1, wxEXPAND | wxRIGHT, sz::RowGap);
    return val;
}

DashboardPage::DashboardPage(wxWindow* parent, PharmacyWorkerThread* worker)
    : BasePage(parent, worker)
    , m_req_low_stock(-1), m_req_sync(-1), m_req_period(-1)
{
    BuildUI();
}

void DashboardPage::BuildUI() {
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(sz::SectionGap);

    // ── KPI row ───────────────────────────────────────────────────────────────
    wxBoxSizer* kpi_row = new wxBoxSizer(wxHORIZONTAL);
    m_lbl_sales  = MakeKpiCard(this, kpi_row, "THIS MONTH SALES", clr::Brand());
    m_lbl_bills  = MakeKpiCard(this, kpi_row, "TOTAL BILLS",      clr::Success());
    m_lbl_alerts = MakeKpiCard(this, kpi_row, "LOW STOCK ITEMS",  clr::Warning());
    m_lbl_sync   = MakeKpiCard(this, kpi_row, "PENDING SYNC",     clr::Text3());
    root->Add(kpi_row, 0, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);

    root->AddSpacer(sz::SectionGap);

    // ── Chart + alerts row ────────────────────────────────────────────────────
    wxBoxSizer* mid_row = new wxBoxSizer(wxHORIZONTAL);

    // Sales chart card
    {
        wxPanel* card = new wxPanel(this);
        card->SetBackgroundColour(clr::Surface());
        wxBoxSizer* cv = new wxBoxSizer(wxVERTICAL);
        cv->AddSpacer(sz::CardPad);
        wxFont tf; tf.SetPointSize(sz::FontPt); tf.MakeBold();
        wxStaticText* t = new wxStaticText(card, wxID_ANY, "Sales - Last 7 Days");
        t->SetFont(tf); t->SetForegroundColour(clr::Text());
        cv->Add(t, 0, wxLEFT, sz::CardPad);
        cv->AddSpacer(8);
        m_chart = new ChartPanel(card, 100);
        cv->Add(m_chart, 1, wxEXPAND | wxALL, sz::CardPad);
        card->SetSizer(cv);
        mid_row->Add(card, 3, wxEXPAND | wxRIGHT, sz::SectionGap);
    }

    // Low-stock alerts card
    {
        wxPanel* card = new wxPanel(this);
        card->SetBackgroundColour(clr::Surface());
        wxBoxSizer* cv = new wxBoxSizer(wxVERTICAL);
        cv->AddSpacer(sz::CardPad);
        wxFont tf; tf.SetPointSize(sz::FontPt); tf.MakeBold();
        wxStaticText* t = new wxStaticText(card, wxID_ANY, "Low Stock Alerts");
        t->SetFont(tf); t->SetForegroundColour(clr::Text());
        cv->Add(t, 0, wxLEFT, sz::CardPad);
        cv->AddSpacer(6);
        m_alert_list = new wxListCtrl(card, wxID_ANY, wxDefaultPosition,
                                      wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
        m_alert_list->AppendColumn("Medicine", wxLIST_FORMAT_LEFT, 140);
        m_alert_list->AppendColumn("Stock",    wxLIST_FORMAT_RIGHT, 50);
        m_alert_list->AppendColumn("Reorder",  wxLIST_FORMAT_RIGHT, 55);
        m_alert_list->SetBackgroundColour(clr::Surface());
        m_alert_list->SetForegroundColour(clr::Text());
        cv->Add(m_alert_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);
        card->SetSizer(cv);
        mid_row->Add(card, 2, wxEXPAND);
    }

    root->Add(mid_row, 1, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
    root->AddSpacer(sz::SectionGap);

    SetSizer(root);
}

void DashboardPage::OnPageShown() {
    // Request fresh data each time the page becomes visible
    {
        DbTask t; t.type = DbCommandType::CMD_GET_LOW_STOCK;
        t.payload = "{}"; t.request_id = next_req_id();
        m_req_low_stock = PostReq(t.request_id);
        m_worker->PostTask(t);
    }
    {
        DbTask t; t.type = DbCommandType::CMD_FLUSH_SYNC;
        t.payload = "{}"; t.request_id = next_req_id();
        m_req_sync = PostReq(t.request_id);
        m_worker->PostTask(t);
    }
    {
        // Current month
        time_t now = time(nullptr);
        struct tm* lt = localtime(&now);
        char month[8];
        std::snprintf(month, sizeof(month), "%04d-%02d", lt->tm_year + 1900, lt->tm_mon + 1);
        DbTask t; t.type = DbCommandType::CMD_GET_PERIOD_SALES;
        t.payload = "{\"month\":\"" + std::string(month) + "\"}";
        t.request_id = next_req_id();
        m_req_period = PostReq(t.request_id);
        m_worker->PostTask(t);
    }
}

void DashboardPage::HandleDbResult(long long req_id, const std::string& json) {
    if (req_id == m_req_low_stock) {
        ParseLowStock(json);
    } else if (req_id == m_req_sync) {
        std::string cnt = jsonutil::extract_str(json, "pending_sync_count");
        m_lbl_sync->SetLabel(wxString::FromUTF8(cnt.empty() ? "0" : cnt.c_str()));
    } else if (req_id == m_req_period) {
        ParsePeriodSales(json);
    }
    Layout();
}

void DashboardPage::ParseLowStock(const std::string& json) {
    m_alert_list->DeleteAllItems();
    if (json.empty() || json == "[]") {
        m_lbl_alerts->SetLabel("0");
        return;
    }

    // Count items by counting '{' at top level
    int count = 0;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);
        std::string name  = jsonutil::extract_str(obj, "name");
        std::string stock = jsonutil::extract_str(obj, "total_stock");
        std::string reord = jsonutil::extract_str(obj, "reorder_level");
        long idx = m_alert_list->InsertItem(count, wxString::FromUTF8(name.c_str()));
        m_alert_list->SetItem(idx, 1, wxString::FromUTF8(stock.c_str()));
        m_alert_list->SetItem(idx, 2, wxString::FromUTF8(reord.c_str()));
        m_alert_list->SetItemBackgroundColour(idx, clr::WarningBg());
        ++count;
        pos = end + 1;
    }
    char buf[8]; std::snprintf(buf, sizeof(buf), "%d", count);
    m_lbl_alerts->SetLabel(buf);
}

void DashboardPage::ParsePeriodSales(const std::string& json) {
    std::vector<ChartBar> bars;
    double total = 0; int count = 0;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);
        std::string date = jsonutil::extract_str(obj, "date");
        double val = std::stod("0" + jsonutil::extract_str(obj, "total"));
        int cnt = std::stoi("0" + jsonutil::extract_str(obj, "count"));
        // Short label: "20 Jun"
        std::string lbl = (date.size() >= 7)
            ? date.substr(8, 2) + "/" + date.substr(5, 2) : date;
        bars.push_back({lbl, val});
        total += val; count += cnt;
        pos = end + 1;
    }
    m_chart->SetBars(bars);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "Rs %.0f", total);
    m_lbl_sales->SetLabel(buf);
    std::snprintf(buf, sizeof(buf), "%d", count);
    m_lbl_bills->SetLabel(buf);
}
