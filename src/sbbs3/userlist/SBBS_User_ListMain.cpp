/***************************************************************
 * Name:      SBBS_User_ListMain.cpp
 * Purpose:   Code for Application Frame
 * Author:    Stephen Hurd (sysop@nix.synchro.net)
 * Created:   2011-10-23
 * Copyright: Stephen Hurd (http://www.synchro.net/)
 * License:
 **************************************************************/

#include "SBBS_User_ListMain.h"
#include <wx/msgdlg.h>

//(*InternalHeaders(SBBS_User_ListFrame)
#include <wx/intl.h>
#include <wx/string.h>
//*)

//helper functions
enum wxbuildinfoformat {
    short_f, long_f };

wxString wxbuildinfo(wxbuildinfoformat format)
{
    wxString wxbuild(wxVERSION_STRING);

    if (format == long_f )
    {
#if defined(__WXMSW__)
        wxbuild << _T("-Windows");
#elif defined(__UNIX__)
        wxbuild << _T("-Linux");
#endif

#if wxUSE_UNICODE
        wxbuild << _T("-Unicode build");
#else
        wxbuild << _T("-ANSI build");
#endif // wxUSE_UNICODE
    }

    return wxbuild;
}

//(*IdInit(SBBS_User_ListFrame)
const long SBBS_User_ListFrame::ID_STATICTEXT1 = wxNewId();
const long SBBS_User_ListFrame::ID_ARSTEXTCTRL = wxNewId();
const long SBBS_User_ListFrame::ID_CLEARBUTTON = wxNewId();
const long SBBS_User_ListFrame::ID_USERLISTCTRL = wxNewId();
const long SBBS_User_ListFrame::ID_STATICTEXT2 = wxNewId();
const long SBBS_User_ListFrame::ID_CHOICE1 = wxNewId();
const long SBBS_User_ListFrame::ID_REFRESHBUTTON = wxNewId();
const long SBBS_User_ListFrame::ID_EDITBUTTON = wxNewId();
const long SBBS_User_ListFrame::idMenuQuit = wxNewId();
const long SBBS_User_ListFrame::idMenuAbout = wxNewId();
const long SBBS_User_ListFrame::ID_STATUSBAR1 = wxNewId();
//*)

BEGIN_EVENT_TABLE(SBBS_User_ListFrame,wxFrame)
    //(*EventTable(SBBS_User_ListFrame)
    //*)
END_EVENT_TABLE()

void SBBS_User_ListFrame::fillUserList(void)
{
    int         totalusers=lastuser(&App->cfg);
    int         i;
    user_t      user;
    int         item;
    wxString    buf;
    char        datebuf[9];

    UserList->DeleteAllItems();
    for(i=0; i<totalusers; i++) {
        user.number=i;
        if(getuserdat(&App->cfg, &user)!=0)
            continue;
        if(ars!=NULL && ars != nular) {
            if(!chk_ar(&App->cfg, ars, &user, NULL))
                continue;
        }
        buf.Printf(_("%d"), user.number);
        item=UserList->InsertItem(i, buf, 0);
        if(user.misc & DELETED) {
            UserList->SetItemTextColour(item, *wxLIGHT_GREY);
        }

        UserList->SetItem(item, 1, wxString::From8BitData(user.alias));
        UserList->SetItem(item, 2, wxString::From8BitData(user.name));
        buf.Printf(_("%d"), user.level);
        UserList->SetItem(item, 3, buf);
        buf.Printf(_("%d"), getage(&App->cfg, user.birth));
        UserList->SetItem(item, 4, buf);
        buf.Printf(_("%c"), user.sex);
        UserList->SetItem(item, 5, buf);
        UserList->SetItem(item, 6, wxString::From8BitData(user.location));
        UserList->SetItem(item, 7, wxString::From8BitData(user.modem));
        UserList->SetItem(item, 8, wxString::From8BitData(user.note));
        UserList->SetItem(item, 9, wxString::From8BitData(user.comp));
        UserList->SetItem(item,10, wxString::From8BitData(user.phone));
        UserList->SetItem(item,11, wxString::From8BitData(user.netmail));
        buf.Printf(_("%d"), user.logons);
        UserList->SetItem(item,12, buf);
        unixtodstr(&App->cfg, user.firston, datebuf);
        UserList->SetItem(item,13, wxString::From8BitData(datebuf));
        unixtodstr(&App->cfg, user.laston, datebuf);
        UserList->SetItem(item,14, wxString::From8BitData(datebuf));
    }
}

SBBS_User_ListFrame::SBBS_User_ListFrame(wxWindow* parent,wxWindowID id)
{
    //(*Initialize(SBBS_User_ListFrame)
    wxBoxSizer* BoxSizer4;
    wxBoxSizer* BoxSizer5;
    wxMenuItem* MenuItem2;
    wxMenuItem* MenuItem1;
    wxBoxSizer* BoxSizer2;
    wxMenu* Menu1;
    wxBoxSizer* BoxSizer1;
    wxMenuBar* MenuBar1;
    wxBoxSizer* BoxSizer3;
    wxMenu* Menu2;

    Create(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, _T("id"));
    BoxSizer1 = new wxBoxSizer(wxVERTICAL);
    BoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
    StaticText1 = new wxStaticText(this, ID_STATICTEXT1, _("ARS Filter"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
    BoxSizer2->Add(StaticText1, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5);
    ARSFilter = new wxTextCtrl(this, ID_ARSTEXTCTRL, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_ARSTEXTCTRL"));
    ARSFilter->SetToolTip(_("Enter an ARS string to filter users with"));
    BoxSizer2->Add(ARSFilter, 1, wxALL|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    ClearButton = new wxButton(this, ID_CLEARBUTTON, _("Clear"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CLEARBUTTON"));
    ClearButton->SetToolTip(_("Clears the ARS filter"));
    BoxSizer2->Add(ClearButton, 0, wxALL|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    BoxSizer1->Add(BoxSizer2, 0, wxALL|wxEXPAND|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    UserList = new wxListCtrl(this, ID_USERLISTCTRL, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_HRULES, wxDefaultValidator, _T("ID_USERLISTCTRL"));
    UserList->InsertColumn(0, wxString(_("Num")));
    UserList->InsertColumn(1, wxString(_("Alias")));
    UserList->InsertColumn(2, wxString(_("Name")));
    UserList->InsertColumn(3, wxString(_("Level")));
    UserList->InsertColumn(4, wxString(_("Age")));
    UserList->InsertColumn(5, wxString(_("Sex")));
    UserList->InsertColumn(6, wxString(_("Location")));
    UserList->InsertColumn(7, wxString(_("Protocol")));
    UserList->InsertColumn(8, wxString(_("Address")));
    UserList->InsertColumn(9, wxString(_("Host Name")));
    UserList->InsertColumn(10, wxString(_("Phone")));
    UserList->InsertColumn(11, wxString(_("Email")));
    UserList->InsertColumn(12, wxString(_("Logons")));
    UserList->InsertColumn(13, wxString(_("First On")));
    UserList->InsertColumn(14, wxString(_("Last On")));
    fillUserList();
    BoxSizer1->Add(UserList, 1, wxALL|wxEXPAND|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    BoxSizer3 = new wxBoxSizer(wxHORIZONTAL);
    BoxSizer4 = new wxBoxSizer(wxHORIZONTAL);
    StaticText2 = new wxStaticText(this, ID_STATICTEXT2, _("Quick Validation Sets"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT2"));
    BoxSizer4->Add(StaticText2, 0, wxALL|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    Choice1 = new wxChoice(this, ID_CHOICE1, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE1"));
    Choice1->SetSelection( Choice1->Append(_("Select a set")) );
    Choice1->Append(_("1 SL:10 F1:                          "));
    Choice1->Append(_("2 SL:20 F1:                          "));
    Choice1->Append(_("3 SL:30 F1:                          "));
    Choice1->Append(_("4 SL:40 F1:                          "));
    Choice1->Append(_("5 SL:50 F1:                          "));
    Choice1->Append(_("6 SL:60 F1:                          "));
    Choice1->Append(_("7 SL:70 F1:                          "));
    Choice1->Append(_("8 SL:80 F1:                          "));
    Choice1->Append(_("9 SL:90 F1:                          "));
    Choice1->Append(_("10 SL:100 F1:                          "));
    Choice1->Disable();
    BoxSizer4->Add(Choice1, 0, wxALL|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    BoxSizer3->Add(BoxSizer4, 1, wxALL|wxEXPAND|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    BoxSizer5 = new wxBoxSizer(wxHORIZONTAL);
    RefreshButton = new wxButton(this, ID_REFRESHBUTTON, _("Refresh"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_REFRESHBUTTON"));
    RefreshButton->SetToolTip(_("Reloads the user database"));
    BoxSizer5->Add(RefreshButton, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    EditButton = new wxButton(this, ID_EDITBUTTON, _("Edit"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_EDITBUTTON"));
    EditButton->Disable();
    BoxSizer5->Add(EditButton, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    BoxSizer3->Add(BoxSizer5, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    BoxSizer1->Add(BoxSizer3, 0, wxALL|wxEXPAND|wxALIGN_TOP|wxALIGN_BOTTOM, 5);
    SetSizer(BoxSizer1);
    MenuBar1 = new wxMenuBar();
    Menu1 = new wxMenu();
    MenuItem1 = new wxMenuItem(Menu1, idMenuQuit, _("Quit\tAlt-F4"), _("Quit the application"), wxITEM_NORMAL);
    Menu1->Append(MenuItem1);
    MenuBar1->Append(Menu1, _("&File"));
    Menu2 = new wxMenu();
    MenuItem2 = new wxMenuItem(Menu2, idMenuAbout, _("About\tF1"), _("Show info about this application"), wxITEM_NORMAL);
    Menu2->Append(MenuItem2);
    MenuBar1->Append(Menu2, _("Help"));
    SetMenuBar(MenuBar1);
    StatusBar1 = new wxStatusBar(this, ID_STATUSBAR1, 0, _T("ID_STATUSBAR1"));
    int __wxStatusBarWidths_1[1] = { -1 };
    int __wxStatusBarStyles_1[1] = { wxSB_NORMAL };
    StatusBar1->SetFieldsCount(1,__wxStatusBarWidths_1);
    StatusBar1->SetStatusStyles(1,__wxStatusBarStyles_1);
    SetStatusBar(StatusBar1);
    BoxSizer1->Fit(this);
    BoxSizer1->SetSizeHints(this);

    Connect(ID_ARSTEXTCTRL,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnARSFilterText);
    Connect(ID_CLEARBUTTON,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnClearButtonClick);
    Connect(ID_REFRESHBUTTON,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnRefreshButtonClick);
    Connect(idMenuQuit,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnQuit);
    Connect(idMenuAbout,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnAbout);
    //*)

    /*
     * Ideally, this would go right after UserList is created
     * and before it's added to the parent.
     */

    if(UserList->GetColumnCount()==0) {
    fprintf(stderr,"No columns in UserList!\r\n");
    UserList->InsertColumn(0, wxString(_("Num")));
    UserList->InsertColumn(1, wxString(_("Alias")));
    UserList->InsertColumn(2, wxString(_("Name")));
    UserList->InsertColumn(3, wxString(_("Level")));
    UserList->InsertColumn(4, wxString(_("Age")));
    UserList->InsertColumn(5, wxString(_("Sex")));
    UserList->InsertColumn(6, wxString(_("Location")));
    UserList->InsertColumn(7, wxString(_("Protocol")));
    UserList->InsertColumn(8, wxString(_("Address")));
    UserList->InsertColumn(9, wxString(_("Host Name")));
    UserList->InsertColumn(10, wxString(_("Phone")));
    UserList->InsertColumn(11, wxString(_("Email")));
    UserList->InsertColumn(12, wxString(_("Logons")));
    UserList->InsertColumn(13, wxString(_("First On")));
    UserList->InsertColumn(14, wxString(_("Last On")));
    fillUserList();
    }
}

SBBS_User_ListFrame::~SBBS_User_ListFrame()
{
    //(*Destroy(SBBS_User_ListFrame)
    //*)
}

void SBBS_User_ListFrame::OnQuit(wxCommandEvent& event)
{
    Close();
}

void SBBS_User_ListFrame::OnAbout(wxCommandEvent& event)
{
    wxString msg = wxbuildinfo(long_f);
    wxMessageBox(msg, _("Welcome to..."));
}

void SBBS_User_ListFrame::OnRefreshButtonClick(wxCommandEvent& event)
{
    fillUserList();
}

void SBBS_User_ListFrame::OnARSFilterText(wxCommandEvent& event)
{
    static uchar    *last_ars=NULL;
    static ushort   last_ars_count=0;
    ushort          count;

    if(!ARSFilter->IsModified())
        return;

    ars=arstr(&count, ARSFilter->GetValue().mb_str(wxConvUTF8), &App->cfg);
    if(count != last_ars_count || memcmp(last_ars, ars, count)) {
        if(last_ars != nular)
            FREE_AND_NULL(last_ars);
        last_ars=ars;
        last_ars_count=count;
        fillUserList();
    }
}

void SBBS_User_ListFrame::OnClearButtonClick(wxCommandEvent& event)
{
    ARSFilter->SetValue(_(""));
    OnARSFilterText(event);
}
