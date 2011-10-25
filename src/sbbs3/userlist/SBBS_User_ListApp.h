/***************************************************************
 * Name:      SBBS_User_ListApp.h
 * Purpose:   Defines Application Class
 * Author:    Stephen Hurd (sysop@nix.synchro.net)
 * Created:   2011-10-23
 * Copyright: Stephen Hurd (http://www.synchro.net/)
 * License:
 **************************************************************/

#ifndef SBBS_USER_LISTAPP_H
#define SBBS_USER_LISTAPP_H

#include <wx/app.h>
#include <sbbs.h>
#undef AST          // TODO: Hack hack... conflicts with wxDateTime
#undef ADT
#undef EST
#undef EDT
#undef CST
#undef CDT
#undef MST
#undef MDT
#undef PST
#undef PDT
#undef HST
#undef HDT
#undef BST
#undef BDT
#undef eof

class SBBS_User_ListApp : public wxApp
{
    public:
        virtual bool OnInit();
        scfg_t  cfg;
};

DECLARE_APP(SBBS_User_ListApp);
extern SBBS_User_ListApp *App;

#endif // SBBS_USER_LISTAPP_H
