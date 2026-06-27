// =============================================================================
// CustomersPage.cpp
// =============================================================================
#include "CustomersPage.h"
#include "AppColors.h"
#include "WorkerThread.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

enum { CP_TXT = wxID_HIGHEST + 300, CP_TIMER };

wxBEGIN_EVENT_TABLE(CustomersPage, BasePage)
    EVT_TEXT(CP_TXT, CustomersPage::OnText)
    EVT_TIMER(CP_TIMER, CustomersPage::OnTimer)
wxEND_EVENT_TABLE()

CustomersPage::CustomersPage(wxWindow* parent, PharmacyWorkerThread* worker)
    : BasePage(parent, worker), m_timer(this, CP_TIMER), m_req(-1)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(sz::CardPad);

    wxBoxSizer* hdr = new wxBoxSizer(wxHORIZONTAL);
    wxFont tf; tf.SetPointSize(sz::FontPt); tf.MakeBold();
    wxStaticText* t = new wxStaticText(this, wxID_ANY, "Customers");
    t->SetFont(tf); t->SetForegroundColour(clr::Text());
    hdr->Add(t, 1, wxALIGN_CENTER_VERTICAL);
    m_txt = new wxTextCtrl(this, CP_TXT, "", wxDefaultPosition, wxSize(220, sz::InputH));
    m_txt->SetHint("Search by name or phone…");
    hdr->Add(m_txt, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(hdr, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
                            wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
    m_list->SetBackgroundColour(clr::Surface());
    m_list->SetForegroundColour(clr::Text());
    m_list->AppendColumn("Name",    wxLIST_FORMAT_LEFT, 180);
    m_list->AppendColumn("Phone",   wxLIST_FORMAT_LEFT, 120);
    m_list->AppendColumn("Address", wxLIST_FORMAT_LEFT, 200);
    m_list->AppendColumn("GSTIN",   wxLIST_FORMAT_LEFT, 130);
    m_list->AppendColumn("Type",    wxLIST_FORMAT_LEFT,  55);
    root->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
    root->AddSpacer(sz::CardPad);
    SetSizer(root);
}

void CustomersPage::OnPageShown() {
    DbTask t; t.type = DbCommandType::CMD_SEARCH_CUSTOMER;
    t.payload = "{\"query\":\"\"}"; t.request_id = next_req_id();
    m_req = PostReq(t.request_id); m_worker->PostTask(t);
}

void CustomersPage::OnText(wxCommandEvent&) {
    m_timer.Stop(); m_timer.StartOnce(280);
}

void CustomersPage::OnTimer(wxTimerEvent&) {
    DbTask t; t.type = DbCommandType::CMD_SEARCH_CUSTOMER;
    t.payload = "{\"query\":\"" + std::string(m_txt->GetValue().utf8_str()) + "\"}";
    t.request_id = next_req_id(); m_req = PostReq(t.request_id); m_worker->PostTask(t);
}

void CustomersPage::HandleDbResult(long long req_id, const std::string& json) {
    if (req_id != m_req) return;
    m_list->DeleteAllItems();
    int n = 0;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos); if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);
        std::string gstin = jsonutil::extract_str(obj, "gstin");
        long idx = m_list->InsertItem(n,
            wxString::FromUTF8(jsonutil::extract_str(obj,"name").c_str()));
        m_list->SetItem(idx, 1, wxString::FromUTF8(jsonutil::extract_str(obj,"phone").c_str()));
        m_list->SetItem(idx, 2, wxString::FromUTF8(jsonutil::extract_str(obj,"address").c_str()));
        m_list->SetItem(idx, 3, wxString::FromUTF8(gstin.c_str()));
        m_list->SetItem(idx, 4, gstin.empty() ? "B2C" : "B2B");
        if (!gstin.empty()) m_list->SetItemBackgroundColour(idx, clr::BrandLight());
        ++n; pos = end + 1;
    }
}
