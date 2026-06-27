// =============================================================================
// MedicinesPage.cpp
// =============================================================================
#include "MedicinesPage.h"
#include "AppColors.h"
#include "WorkerThread.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

static const char* kSchedLabel[] = { "OTC", "Sch H", "Sch H1", "Sch X" };

enum { MP_TXT_SEARCH = wxID_HIGHEST + 200, MP_TIMER };

wxBEGIN_EVENT_TABLE(MedicinesPage, BasePage)
    EVT_TEXT(MP_TXT_SEARCH, MedicinesPage::OnSearchText)
    EVT_TIMER(MP_TIMER,     MedicinesPage::OnTimer)
wxEND_EVENT_TABLE()

MedicinesPage::MedicinesPage(wxWindow* parent, PharmacyWorkerThread* worker)
    : BasePage(parent, worker), m_timer(this, MP_TIMER), m_req(-1)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(sz::CardPad);

    wxBoxSizer* top = new wxBoxSizer(wxHORIZONTAL);
    wxFont tf; tf.SetPointSize(sz::FontPt); tf.MakeBold();
    wxStaticText* t = new wxStaticText(this, wxID_ANY, "Medicine Catalogue");
    t->SetFont(tf); t->SetForegroundColour(clr::Text());
    top->Add(t, 1, wxALIGN_CENTER_VERTICAL);
    m_txt_search = new wxTextCtrl(this, MP_TXT_SEARCH, "",
        wxDefaultPosition, wxSize(220, sz::InputH));
    m_txt_search->SetHint("Search by name or generic…");
    top->Add(m_txt_search, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(top, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
                            wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
    m_list->SetBackgroundColour(clr::Surface());
    m_list->SetForegroundColour(clr::Text());
    m_list->AppendColumn("Name",         wxLIST_FORMAT_LEFT, 200);
    m_list->AppendColumn("Generic",      wxLIST_FORMAT_LEFT, 160);
    m_list->AppendColumn("Manufacturer", wxLIST_FORMAT_LEFT, 130);
    m_list->AppendColumn("Schedule",     wxLIST_FORMAT_LEFT, 70);
    m_list->AppendColumn("HSN",          wxLIST_FORMAT_LEFT, 80);
    m_list->AppendColumn("GST%",         wxLIST_FORMAT_RIGHT, 50);
    root->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
    root->AddSpacer(sz::CardPad);
    SetSizer(root);
}

void MedicinesPage::OnPageShown() {
    DbTask t; t.type = DbCommandType::CMD_SEARCH_MEDICINE;
    t.payload = "{\"query\":\"\"}"; t.request_id = next_req_id();
    m_req = PostReq(t.request_id); m_worker->PostTask(t);
}

void MedicinesPage::OnSearchText(wxCommandEvent&) {
    m_timer.Stop(); m_timer.StartOnce(280);
}

void MedicinesPage::OnTimer(wxTimerEvent&) {
    DbTask t; t.type = DbCommandType::CMD_SEARCH_MEDICINE;
    t.payload = "{\"query\":\"" +
        std::string(m_txt_search->GetValue().utf8_str()) + "\"}";
    t.request_id = next_req_id();
    m_req = PostReq(t.request_id); m_worker->PostTask(t);
}

void MedicinesPage::HandleDbResult(long long req_id, const std::string& json) {
    if (req_id != m_req) return;
    m_list->DeleteAllItems();
    int n = 0;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos); if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);
        int sched = (int)jsonutil::extract_int(obj, "is_scheduled");
        if (sched < 0 || sched > 3) sched = 0;
        long idx = m_list->InsertItem(n,
            wxString::FromUTF8(jsonutil::extract_str(obj,"name").c_str()));
        m_list->SetItem(idx, 1, wxString::FromUTF8(jsonutil::extract_str(obj,"generic_name").c_str()));
        m_list->SetItem(idx, 3, kSchedLabel[sched]);
        m_list->SetItem(idx, 4, wxString::FromUTF8(jsonutil::extract_str(obj,"hsn_code").c_str()));
        m_list->SetItem(idx, 5, wxString::FromUTF8(jsonutil::extract_str(obj,"gst_rate").c_str()));
        // Colour schedule rows
        if (sched == 3) m_list->SetItemBackgroundColour(idx, clr::DangerBg());
        else if (sched == 2) m_list->SetItemBackgroundColour(idx, clr::SchedH1Bg());
        else if (sched == 1) m_list->SetItemBackgroundColour(idx, clr::WarningBg());
        ++n; pos = end + 1;
    }
}
