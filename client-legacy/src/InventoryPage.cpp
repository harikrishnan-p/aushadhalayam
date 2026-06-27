// =============================================================================
// InventoryPage.cpp
// =============================================================================
#include "InventoryPage.h"
#include "AppColors.h"
#include "WorkerThread.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

static wxListCtrl* MakeList(wxWindow* parent, wxBoxSizer* sizer,
                             const wxString& title,
                             std::initializer_list<std::pair<wxString,int>> cols)
{
    wxFont tf; tf.SetPointSize(sz::FontPt); tf.MakeBold();
    wxStaticText* t = new wxStaticText(parent, wxID_ANY, title);
    t->SetFont(tf); t->SetForegroundColour(clr::Text());
    sizer->Add(t, 0, wxLEFT | wxBOTTOM, sz::CardPad);

    wxListCtrl* lc = new wxListCtrl(parent, wxID_ANY, wxDefaultPosition,
                                    wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
    lc->SetBackgroundColour(clr::Surface());
    lc->SetForegroundColour(clr::Text());
    for (auto& c : cols)
        lc->AppendColumn(c.first, wxLIST_FORMAT_LEFT, c.second);
    sizer->Add(lc, 1, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
    return lc;
}

InventoryPage::InventoryPage(wxWindow* parent, PharmacyWorkerThread* worker)
    : BasePage(parent, worker), m_req_inv(-1), m_req_alert(-1)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(sz::CardPad);

    m_alert_list = MakeList(this, root, "Low Stock Alerts", {
        {"Medicine", 200}, {"Stock", 60}, {"Reorder", 70}
    });
    m_alert_list->SetMinSize(wxSize(-1, 100));

    root->AddSpacer(sz::SectionGap);

    m_list = MakeList(this, root, "Stock Batches (FEFO)", {
        {"Medicine", 180}, {"Batch", 90}, {"Expiry", 70},
        {"Qty", 55}, {"MRP", 70}, {"Purchase", 70}, {"Margin%", 65}
    });

    root->AddSpacer(sz::CardPad);
    SetSizer(root);
}

void InventoryPage::OnPageShown() {
    { DbTask t; t.type = DbCommandType::CMD_GET_LOW_STOCK; t.payload = "{}";
      t.request_id = next_req_id(); m_req_alert = PostReq(t.request_id);
      m_worker->PostTask(t); }
    { DbTask t; t.type = DbCommandType::CMD_GET_INVENTORY; t.payload = "{}";
      t.request_id = next_req_id(); m_req_inv = PostReq(t.request_id);
      m_worker->PostTask(t); }
}

void InventoryPage::HandleDbResult(long long req_id, const std::string& json) {
    if (req_id == m_req_alert) {
        m_alert_list->DeleteAllItems();
        int n = 0;
        size_t pos = 0;
        while ((pos = json.find('{', pos)) != std::string::npos) {
            size_t end = json.find('}', pos); if (end == std::string::npos) break;
            std::string obj = json.substr(pos, end - pos + 1);
            long idx = m_alert_list->InsertItem(n,
                wxString::FromUTF8(jsonutil::extract_str(obj,"name").c_str()));
            m_alert_list->SetItem(idx, 1, wxString::FromUTF8(jsonutil::extract_str(obj,"total_stock").c_str()));
            m_alert_list->SetItem(idx, 2, wxString::FromUTF8(jsonutil::extract_str(obj,"reorder_level").c_str()));
            m_alert_list->SetItemBackgroundColour(idx, clr::WarningBg());
            ++n; pos = end + 1;
        }
        return;
    }
    if (req_id == m_req_inv) {
        m_list->DeleteAllItems();
        int n = 0;
        size_t pos = 0;
        while ((pos = json.find('{', pos)) != std::string::npos) {
            size_t end = json.find('}', pos); if (end == std::string::npos) break;
            std::string obj = json.substr(pos, end - pos + 1);
            std::string exp = jsonutil::extract_str(obj, "expiry_date");
            // Expiry badge colour
            wxColour bg = clr::Surface();
            if (exp < [](){ time_t t=time(nullptr); struct tm* l=localtime(&t);
                char b[8]; std::snprintf(b,sizeof(b),"%04d-%02d",l->tm_year+1900,l->tm_mon+1);
                return std::string(b); }()) bg = clr::DangerBg();

            long idx = m_list->InsertItem(n, wxString::FromUTF8(jsonutil::extract_str(obj,"product_name").c_str()));
            m_list->SetItem(idx, 1, wxString::FromUTF8(jsonutil::extract_str(obj,"batch_number").c_str()));
            m_list->SetItem(idx, 2, wxString::FromUTF8(exp.c_str()));
            m_list->SetItem(idx, 3, wxString::FromUTF8(jsonutil::extract_str(obj,"quantity").c_str()));
            m_list->SetItem(idx, 4, wxString::FromUTF8(jsonutil::extract_str(obj,"mrp").c_str()));
            m_list->SetItem(idx, 5, wxString::FromUTF8(jsonutil::extract_str(obj,"purchase_rate").c_str()));
            m_list->SetItem(idx, 6, wxString::FromUTF8(jsonutil::extract_str(obj,"margin").c_str()));
            if (bg != clr::Surface()) m_list->SetItemBackgroundColour(idx, bg);
            ++n; pos = end + 1;
        }
    }
}
