/***************************************************************
 * Name:      SBBS_User_ListApp.cpp
 * Purpose:   Code for Application Class
 * Author:    Stephen Hurd (sysop@nix.synchro.net)
 * Created:   2011-10-23
 * Copyright: Stephen Hurd (http://www.synchro.net/)
 * License:
 **************************************************************/

#include "SBBS_User_ListApp.h"

//(*AppHeaders
#include "SBBS_User_ListMain.h"
#include <wx/image.h>
//*)
#include <wx/msgdlg.h>
#include <wx/strconv.h>

IMPLEMENT_APP(SBBS_User_ListApp);

extern "C" int lprintf(int level, const char *fmt, ...) /* log output */
{
    return 0;
}

SBBS_User_ListApp *App;

bool SBBS_User_ListApp::OnInit()
{    wxString    ctrlDir;
    char        errstr[1024];

    App=this;

    /* Check config... */
    if(!wxGetEnv(_("SBBSCTRL"), &ctrlDir)) {
        wxMessageDialog *dlg = new wxMessageDialog(NULL, _("SBBSCTRL environment variable not set.  This variable must be set to the ctrl directory (ie: /sbbs/ctrl)."), _("SBBSCTRL Not Set"));
        dlg->ShowModal();
        return false;
    }
    memset(&cfg, 0, sizeof(cfg));
    cfg.size=sizeof(cfg);
    SAFECOPY(cfg.ctrl_dir, ctrlDir.mb_str(wxConvUTF8));
    prep_dir("", cfg.ctrl_dir, sizeof(cfg.ctrl_dir));
    if(!isdir(cfg.ctrl_dir)) {
        wxMessageDialog *dlg = new wxMessageDialog(NULL, _("SBBSCTRL environment variable is not set to a directory.  This variable must be set to the ctrl directory (ie: /sbbs/ctrl)."), _("SBBSCTRL Not A Directory"));
        dlg->ShowModal();
        return false;
    }
    if(!load_cfg(&cfg, NULL, TRUE, errstr)) {
        wxString str(errstr,wxConvUTF8);
        wxMessageDialog *dlg = new wxMessageDialog(NULL, _("ERROR: \"")+str+_("\"loading config"), _("Config Error"));
        dlg->ShowModal();
        return false;
    }
    //(*AppInitialize
    bool wxsOK = true;
    wxInitAllImageHandlers();
    if ( wxsOK )
    {
    SBBS_User_ListFrame* Frame = new SBBS_User_ListFrame(0);
    Frame->Show();
    SetTopWindow(Frame);
    }
    //*)
    return wxsOK;

}
