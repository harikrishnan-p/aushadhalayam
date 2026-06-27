// =============================================================================
// SalesPage.cpp
// =============================================================================
#include "SalesPage.h"
#include "AppColors.h"
#include "WorkerThread.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <ctime>
#include <cstdio>

SalesPage::SalesPage(wxWindow* parent, PharmacyWorkerThread* worker)
    : BasePage(parent, worker), m_req(-1)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(sz::CardPad);

    wxFont tf; tf.SetPointSize(sz::FontPt); tf.MakeBold();
    wxStaticText* t = new wxStaticText(this, wxID_ANY, "Sales Reports - This Month");
    t->SetFont(tf); t->SetForegroundColour(clr::Text());
    root->Add(t, 0, wxLEFT | wxBOTTOM, sz::CardPad);

    // KPI row
    wxBoxSizer* krow = new wxBoxSizer(wxHORIZONTAL);
    auto kpi = [&](const wxString& lbl, wxStaticText*& out) {
        wxPanel* card = new wxPanel(this);
        card->SetBackgroundColour(clr::Surface());
        card->SetMinSize(wxSize(160, 64));
        wxBoxSizer* cv = new wxBoxSizer(wxVERTICAL);
        cv->AddSpacer(8);
        wxFont lf; lf.SetPointSize(8); lf.MakeBold();
        wxStaticText* k = new wxStaticText(card, wxID_ANY, lbl);
        k->SetFont(lf); k->SetForegroundColour(clr::Text2());
        cv->Add(k, 0, wxLEFT, sz::CardPad);
        wxFont vf; vf.SetPointSize(14); vf.MakeBold();
        out = new wxStaticText(card, wxID_ANY, "0");
        out->SetFont(vf); out->SetForegroundColour(clr::Brand());
        cv->Add(out, 0, wxLEFT, sz::CardPad);
        card->SetSizer(cv);
        krow->Add(card, 1, wxEXPAND | wxRIGHT, sz::RowGap);
    };
    kpi("TOTAL SALES", m_lbl_total);
    kpi("TOTAL BILLS", m_lbl_bills);
    root->Add(krow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);

    // Chart
    root->AddSpacer(sz::RowGap);
    wxFont sf; sf.SetPointSize(9); sf.MakeBold();
    wxStaticText* cs = new wxStaticText(this, wxID_ANY, "Daily Revenue");
    cs->SetFont(sf); cs->SetForegroundColour(clr::Text());
    root->Add(cs, 0, wxLEFT | wxBOTTOM, sz::CardPad);
    m_chart = new ChartPanel(this, 110);
    root->Add(m_chart, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);

    root->AddSpacer(sz::RowGap);
    wxStaticText* ls = new wxStaticText(this, wxID_ANY, "Daily Breakdown");
    ls->SetFont(sf); ls->SetForegroundColour(clr::Text());
    root->Add(ls, 0, wxLEFT | wxBOTTOM, sz::CardPad);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
                            wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
    m_list->SetBackgroundColour(clr::Surface());
    m_list->SetForegroundColour(clr::Text());
    m_list->AppendColumn("Date",  wxLIST_FORMAT_LEFT,  100);
    m_list->AppendColumn("Revenue", wxLIST_FORMAT_RIGHT, 100);
    m_list->AppendColumn("Bills",   wxLIST_FORMAT_RIGHT, 60);
    root->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
    root->AddSpacer(sz::CardPad);
    SetSizer(root);
}

void SalesPage::OnPageShown() {
    time_t now = time(nullptr);
    struct tm* lt = localtime(&now);
    char month[8];
    std::snprintf(month, sizeof(month), "%04d-%02d", lt->tm_year + 1900, lt->tm_mon + 1);
    DbTask t; t.type = DbCommandType::CMD_GET_PERIOD_SALES;
    t.payload = "{\"month\":\"" + std::string(month) + "\"}";
    t.request_id = next_req_id(); m_req = PostReq(t.request_id);
    m_worker->PostTask(t);
}

void SalesPage::HandleDbResult(long long req_id, const std::string& json) {
    if (req_id != m_req) return;
    m_list->DeleteAllItems();
    std::vector<ChartBar> bars;
    double total = 0; int count = 0;
    int n = 0;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos); if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);
        std::string date = jsonutil::extract_str(obj, "date");
        std::string tot  = jsonutil::extract_str(obj, "total");
        std::string cnt  = jsonutil::extract_str(obj, "count");
        double v = tot.empty() ? 0.0 : std::stod(tot);
        int    c = cnt.empty() ? 0   : std::stoi(cnt);
        std::string lbl = (date.size() >= 7) ? date.substr(8,2) + "/" + date.substr(5,2) : date;
        bars.push_back({lbl, v});
        total += v; count += c;
        long idx = m_list->InsertItem(n, wxString::FromUTF8(date.c_str()));
        char buf[32]; std::snprintf(buf, sizeof(buf), "Rs %.2f", v);
        m_list->SetItem(idx, 1, buf);
        std::snprintf(buf, sizeof(buf), "%d", c);
        m_list->SetItem(idx, 2, buf);
        ++n; pos = end + 1;
    }
    m_chart->SetBars(bars);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Rs %.2f", total);
    m_lbl_total->SetLabel(buf);
    std::snprintf(buf, sizeof(buf), "%d", count);
    m_lbl_bills->SetLabel(buf);
}
