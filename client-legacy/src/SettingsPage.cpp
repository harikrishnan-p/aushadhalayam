// =============================================================================
// SettingsPage.cpp
// =============================================================================
#include "SettingsPage.h"
#include "AppColors.h"
#include <wx/sizer.h>
#include <wx/notebook.h>
#include <wx/stattext.h>
#include <wx/checkbox.h>
#include <wx/radiobox.h>
#include <wx/spinctrl.h>

static wxPanel* MakeTab(wxNotebook* nb, const wxString& label) {
    wxPanel* p = new wxPanel(nb);
    p->SetBackgroundColour(clr::Surface());
    nb->AddPage(p, label);
    return p;
}

static void AddRow(wxPanel* p, wxFlexGridSizer* fg,
                   const wxString& label, wxWindow* ctrl)
{
    wxFont lf; lf.SetPointSize(9);
    wxStaticText* k = new wxStaticText(p, wxID_ANY, label);
    k->SetFont(lf); k->SetForegroundColour(clr::Text2());
    fg->Add(k, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, sz::RowGap);
    fg->Add(ctrl, 0, wxEXPAND | wxBOTTOM, sz::RowGap);
}

SettingsPage::SettingsPage(wxWindow* parent, PharmacyWorkerThread* worker)
    : BasePage(parent, worker)
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(sz::CardPad);

    wxFont tf; tf.SetPointSize(sz::FontPt); tf.MakeBold();
    wxStaticText* t = new wxStaticText(this, wxID_ANY, "Settings");
    t->SetFont(tf); t->SetForegroundColour(clr::Text());
    root->Add(t, 0, wxLEFT | wxBOTTOM, sz::CardPad);

    wxNotebook* nb = new wxNotebook(this, wxID_ANY);
    nb->SetBackgroundColour(clr::Surface());

    // ── Appearance tab ────────────────────────────────────────────────────────
    {
        wxPanel* p = MakeTab(nb, "Appearance");
        wxFlexGridSizer* fg = new wxFlexGridSizer(2, wxSize(sz::SectionGap, 0));
        fg->AddGrowableCol(1);
        wxFont lf; lf.SetPointSize(9);
        wxStaticText* n = new wxStaticText(p, wxID_ANY,
            "Theme: Blue (fixed in this build)\n\n"
            "Colour scheme is set at compile time.\n"
            "The modern Tauri build supports 5 themes.");
        n->SetFont(lf); n->SetForegroundColour(clr::Text2());
        n->Wrap(320);
        fg->Add(n, 0, wxBOTTOM, sz::SectionGap);
        fg->AddSpacer(1);

        AddRow(p, fg, "Auto-print after checkout:",
            new wxCheckBox(p, wxID_ANY, ""));
        AddRow(p, fg, "Warn near-expiry (< 90 days):",
            new wxCheckBox(p, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                          0, wxDefaultValidator, "expiry_warn"));

        wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);
        vs->AddSpacer(sz::CardPad);
        vs->Add(fg, 0, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
        p->SetSizer(vs);
    }

    // ── Billing tab ───────────────────────────────────────────────────────────
    {
        wxPanel* p = MakeTab(nb, "Billing");
        wxFlexGridSizer* fg = new wxFlexGridSizer(2, wxSize(sz::SectionGap, 0));
        fg->AddGrowableCol(1);
        AddRow(p, fg, "Default Discount %:",
            new wxSpinCtrlDouble(p, wxID_ANY, "0", wxDefaultPosition,
                                 wxSize(80, sz::InputH), wxSP_ARROW_KEYS, 0, 100, 0, 0.5));
        AddRow(p, fg, "Ask for customer on every bill:",
            new wxCheckBox(p, wxID_ANY, ""));
        AddRow(p, fg, "Payment mode default:",
            new wxChoice(p, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr));
        wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);
        vs->AddSpacer(sz::CardPad);
        vs->Add(fg, 0, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
        p->SetSizer(vs);
    }

    // ── Sync tab ──────────────────────────────────────────────────────────────
    {
        wxPanel* p = MakeTab(nb, "Sync");
        wxFlexGridSizer* fg = new wxFlexGridSizer(2, wxSize(sz::SectionGap, 0));
        fg->AddGrowableCol(1);
        AddRow(p, fg, "Enable cloud sync:",
            new wxCheckBox(p, wxID_ANY, ""));
        AddRow(p, fg, "Sync endpoint URL:",
            new wxTextCtrl(p, wxID_ANY, "https://sync.aushadhalayam.local/api/ingest",
                          wxDefaultPosition, wxSize(260, sz::InputH)));
        AddRow(p, fg, "Sync interval (seconds):",
            new wxSpinCtrl(p, wxID_ANY, "30", wxDefaultPosition,
                          wxSize(80, sz::InputH), wxSP_ARROW_KEYS, 5, 3600, 30));
        wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);
        vs->AddSpacer(sz::CardPad);
        vs->Add(fg, 0, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
        p->SetSizer(vs);
    }

    // ── Hardware tab ──────────────────────────────────────────────────────────
    {
        wxPanel* p = MakeTab(nb, "Hardware");
        wxFlexGridSizer* fg = new wxFlexGridSizer(2, wxSize(sz::SectionGap, 0));
        fg->AddGrowableCol(1);
        wxArrayString ports;
        ports.Add("COM1"); ports.Add("COM2"); ports.Add("COM3");
        ports.Add("USB (auto-detect)");
        AddRow(p, fg, "Printer port:", new wxChoice(p, wxID_ANY,
            wxDefaultPosition, wxDefaultSize, ports));
        wxArrayString widths; widths.Add("58mm"); widths.Add("80mm");
        AddRow(p, fg, "Paper width:", new wxChoice(p, wxID_ANY,
            wxDefaultPosition, wxDefaultSize, widths));
        AddRow(p, fg, "Open cash drawer:", new wxCheckBox(p, wxID_ANY, ""));
        wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);
        vs->AddSpacer(sz::CardPad);
        vs->Add(fg, 0, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
        p->SetSizer(vs);
    }

    // ── GST tab ───────────────────────────────────────────────────────────────
    {
        wxPanel* p = MakeTab(nb, "GST / Tax");
        wxFlexGridSizer* fg = new wxFlexGridSizer(2, wxSize(sz::SectionGap, 0));
        fg->AddGrowableCol(1);
        AddRow(p, fg, "GSTIN:", new wxTextCtrl(p, wxID_ANY, "",
            wxDefaultPosition, wxSize(-1, sz::InputH)));
        AddRow(p, fg, "State Code:", new wxTextCtrl(p, wxID_ANY, "",
            wxDefaultPosition, wxSize(60, sz::InputH)));
        AddRow(p, fg, "Legal Business Name:", new wxTextCtrl(p, wxID_ANY, "",
            wxDefaultPosition, wxSize(-1, sz::InputH)));
        AddRow(p, fg, "Composition Scheme:", new wxCheckBox(p, wxID_ANY, ""));
        wxBoxSizer* vs = new wxBoxSizer(wxVERTICAL);
        vs->AddSpacer(sz::CardPad);
        vs->Add(fg, 0, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);

        wxButton* save = new wxButton(p, wxID_ANY, "Save GST Settings",
            wxDefaultPosition, wxSize(-1, sz::BtnH));
        save->SetBackgroundColour(clr::Brand());
        save->SetForegroundColour(*wxWHITE);
        vs->AddSpacer(sz::SectionGap);
        vs->Add(save, 0, wxLEFT, sz::CardPad);
        p->SetSizer(vs);
    }

    root->Add(nb, 1, wxEXPAND | wxLEFT | wxRIGHT, sz::CardPad);
    root->AddSpacer(sz::CardPad);
    SetSizer(root);
}
