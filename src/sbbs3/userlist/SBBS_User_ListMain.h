/***************************************************************
 * Name:      SBBS_User_ListMain.h
 * Purpose:   Defines Application Frame
 * Author:    Stephen Hurd (sysop@nix.synchro.net)
 * Created:   2011-10-23
 * Copyright: Stephen Hurd (http://www.synchro.net/)
 * License:
 **************************************************************/

#ifndef SBBS_USER_LISTMAIN_H
#define SBBS_USER_LISTMAIN_H

//(*Headers(SBBS_User_ListFrame)
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/menu.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/frame.h>
#include <wx/statusbr.h>
//*)

#include "SBBS_User_ListApp.h"

class SBBS_User_ListFrame: public wxFrame
{
    public:

        SBBS_User_ListFrame(wxWindow* parent,wxWindowID id = -1);
        virtual ~SBBS_User_ListFrame();

    private:

        //(*Handlers(SBBS_User_ListFrame)
        void OnQuit(wxCommandEvent& event);
        void OnAbout(wxCommandEvent& event);
        //*)

        //(*Identifiers(SBBS_User_ListFrame)
        static const long ID_STATICTEXT1;
        static const long ID_TEXTCTRL1;
        static const long ID_CLEARBUTTON;
        static const long ID_USERLISTCTRL;
        static const long ID_STATICTEXT2;
        static const long ID_CHOICE1;
        static const long ID_REFRESHBUTTON;
        static const long ID_EDITBUTTON;
        static const long idMenuQuit;
        static const long idMenuAbout;
        static const long ID_STATUSBAR1;
        //*)

        //(*Declarations(SBBS_User_ListFrame)
        wxButton* RefreshButton;
        wxStaticText* StaticText2;
        wxStaticText* StaticText1;
        wxListCtrl* UserList;
        wxStatusBar* StatusBar1;
        wxButton* ClearButton;
        wxTextCtrl* TextCtrl1;
        wxButton* EditButton;
        wxChoice* Choice1;
        //*)

        DECLARE_EVENT_TABLE()
};

#endif // SBBS_USER_LISTMAIN_H
