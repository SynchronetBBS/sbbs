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
#include <wx/panel.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/frame.h>
//*)

#include "SBBS_User_ListApp.h"

class SBBS_User_ListFrame: public wxFrame
{
    public:

        SBBS_User_ListFrame(wxWindow* parent,wxWindowID id = -1);
        virtual ~SBBS_User_ListFrame();

    private:

        void fillUserList(void);
		void CopyItems(int state);
		void applyARS(void);
		int	sort;

        //(*Handlers(SBBS_User_ListFrame)
        void OnQuit(wxCommandEvent& event);
        void OnAbout(wxCommandEvent& event);
        void OnRefreshButtonClick(wxCommandEvent& event);
        void OnARSFilterText(wxCommandEvent& event);
        void OnClearButtonClick(wxCommandEvent& event);
        void OnUserListItemSelect(wxListEvent& event);
        void OnQVChoiceSelect(wxCommandEvent& event);
        void OnUserListItemRClick(wxListEvent& event);
        void CopyMenuItemSelected(wxCommandEvent& event);
        void CopyAllMenuItemSelected(wxCommandEvent& event);
        void OnUserListColumnClick(wxListEvent& event);
        //*)

        //(*Identifiers(SBBS_User_ListFrame)
        static const long ID_STATICTEXT1;
        static const long ID_ARSTEXTCTRL;
        static const long ID_CLEARBUTTON;
        static const long ID_USERLISTCTRL;
        static const long ID_STATICTEXT2;
        static const long ID_QVCHOICE;
        static const long ID_REFRESHBUTTON;
        static const long ID_EDITBUTTON;
        static const long ID_PANEL1;
        static const long ID_EDITUSER;
        static const long ID_COPY;
        static const long ID_COPYALL;
        static const long ID_REFRESH;
        //*)

        //(*Declarations(SBBS_User_ListFrame)
        wxButton* RefreshButton;
        wxMenuItem* MenuItem5;
        wxStaticText* StaticText2;
        wxMenuItem* MenuItem4;
        wxPanel* Panel1;
        wxStaticText* StaticText1;
        wxMenu ContextMenu;
        wxListCtrl* UserList;
        wxMenuItem* MenuItem3;
        wxMenuItem* MenuItem6;
        wxButton* ClearButton;
        wxChoice* QVChoice;
        wxButton* EditButton;
        wxTextCtrl* ARSFilter;
        //*)

        unsigned char *ars;
        DECLARE_EVENT_TABLE()
};

#endif // SBBS_USER_LISTMAIN_H
