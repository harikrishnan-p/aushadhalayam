// =============================================================================
// PurchasePage.cpp  —  Purchase history (UI-only receive form is future work)
// =============================================================================
#include "PurchasePage.h"
#include "AppColors.h"
#include "WorkerThread.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

PurchasePage::PurchasePage(wxWindow* parent, PharmacyWorkerThread* worker)
    : BasePage(parent, worker), m_req(-1)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(sz::CardPad);

    wxBoxSizer* hdr = new wxBoxSizer(wxHORIZONTAL);
    wxFont tf; tf.SetPointSize(sz::FontPt); tf.MakeBold();
    wxStaticText* t = new wxStaticText(this, wxID_ANY, "Purchase Orders / Received Stock");
    t->SetFont(tf); t->SetForegroundColour(clr::Text());
    hdr->Add(t, 1, wxALIGN_CENTER_VERTICAL);
    // Badge
    wxStaticText* badge = new wxStaticText(this, wxID_ANY, "Receive form - backend coming soon");
    badge->SetForegroundColour(clr::Warning());
    badge->SetBackgroundColour(clr::WarningBg());
    hdr->Add(badge, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(hdr, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, sz::CardPad);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
                            wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
    m_list->SetBackgroundColour(clr::Surface());
    m_list->SetForegroundColour(clr::Text());
    m_list->AppendColumn("Medicine",   wxLIST_FORMAT_LEFT,  180);
    m_list->AppendColumn("Batch",      wxLIST_FORMAT_LEFT,   90);
    m_list->AppendColumn("Expiry",     wxLIST_FORMAT_LEFT,   70);
    m_list->AppendColumn("Qty",        wxLIST_FORMAT_RIGHT,  55);
    m_list->AppendColumn("MRP",        wxLIST_FORMAT_RIGHT,  70);
    m_list->AppendColumn("Purchase",   wxLIST_FORMAT_RIGHT,  70);
    m_list->AppendColumn("Margin%",    wxLIST_FORMAT_RIGHT,  65);
    root->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
    root->AddSpacer(sz::CardPad);
    SetSizer(root);
}

void PurchasePage::OnPageShown() {
    DbTask t; t.type = DbCommandType::CMD_GET_INVENTORY; t.payload = "{}";
    t.request_id = next_req_id(); m_req = PostReq(t.request_id);
    m_worker->PostTask(t);
}

void PurchasePage::HandleDbResult(long long req_id, const std::string& json) {
    if (req_id != m_req) return;
    m_list->DeleteAllItems();
    int n = 0;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t end = json.find('}', pos); if (end == std::string::npos) break;
        std::string obj = json.substr(pos, end - pos + 1);
        long idx = m_list->InsertItem(n,
            wxString::FromUTF8(jsonutil::extract_str(obj,"product_name").c_str()));
        m_list->SetItem(idx, 1, wxString::FromUTF8(jsonutil::extract_str(obj,"batch_number").c_str()));
        m_list->SetItem(idx, 2, wxString::FromUTF8(jsonutil::extract_str(obj,"expiry_date").c_str()));
        m_list->SetItem(idx, 3, wxString::FromUTF8(jsonutil::extract_str(obj,"quantity").c_str()));
        m_list->SetItem(idx, 4, wxString::FromUTF8(jsonutil::extract_str(obj,"mrp").c_str()));
        m_list->SetItem(idx, 5, wxString::FromUTF8(jsonutil::extract_str(obj,"purchase_rate").c_str()));
        m_list->SetItem(idx, 6, wxString::FromUTF8(jsonutil::extract_str(obj,"margin").c_str()));
        m_list->SetItemBackgroundColour(idx, clr::SuccessBg());
        ++n; pos = end + 1;
    }
}
