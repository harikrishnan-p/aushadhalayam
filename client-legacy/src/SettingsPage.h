#pragma once
#include "Pages.h"
#include <wx/notebook.h>
class SettingsPage : public BasePage {
public:
    SettingsPage(wxWindow* parent, PharmacyWorkerThread* worker);
};
