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

//(*InternalHeaders(SBBS_User_ListFrame)#include <wx/intl.h>#include <wx/string.h>//*)

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

//(*IdInit(SBBS_User_ListFrame)const long SBBS_User_ListFrame::ID_STATICTEXT1 = wxNewId();const long SBBS_User_ListFrame::ID_ARSTEXTCTRL = wxNewId();const long SBBS_User_ListFrame::ID_CLEARBUTTON = wxNewId();const long SBBS_User_ListFrame::ID_USERLISTCTRL = wxNewId();const long SBBS_User_ListFrame::ID_STATICTEXT2 = wxNewId();const long SBBS_User_ListFrame::ID_QVCHOICE = wxNewId();const long SBBS_User_ListFrame::ID_REFRESHBUTTON = wxNewId();const long SBBS_User_ListFrame::ID_EDITBUTTON = wxNewId();const long SBBS_User_ListFrame::ID_PANEL1 = wxNewId();const long SBBS_User_ListFrame::idMenuQuit = wxNewId();const long SBBS_User_ListFrame::idMenuAbout = wxNewId();const long SBBS_User_ListFrame::ID_STATUSBAR1 = wxNewId();//*)

BEGIN_EVENT_TABLE(SBBS_User_ListFrame,wxFrame)
    //(*EventTable(SBBS_User_ListFrame)
    //*)
END_EVENT_TABLE()

void SBBS_User_ListFrame::fillUserList(void)
{
    int         totalusers=lastuser(&App->cfg);
    int         i;
    user_t      user;
    int         item=0;
    wxString    buf;
    char        datebuf[9];
    long        topitem=UserList->GetTopItem();

    UserList->Freeze();
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
        UserList->SetItemData(item, user.number);
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
    UserList->EnsureVisible(item);
    UserList->EnsureVisible(topitem);
	UserList->Thaw();
}

SBBS_User_ListFrame::SBBS_User_ListFrame(wxWindow* parent,wxWindowID id)
{
    //(*Initialize(SBBS_User_ListFrame)    wxBoxSizer* BoxSizer4;    wxBoxSizer* BoxSizer5;    wxMenuItem* MenuItem2;    wxMenuItem* MenuItem1;    wxBoxSizer* BoxSizer2;    wxMenu* Menu1;    wxBoxSizer* BoxSizer1;    wxMenuBar* MenuBar1;    wxBoxSizer* BoxSizer3;    wxMenu* Menu2;    Create(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, _T("id"));    Panel1 = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("ID_PANEL1"));    BoxSizer1 = new wxBoxSizer(wxVERTICAL);    BoxSizer2 = new wxBoxSizer(wxHORIZONTAL);    StaticText1 = new wxStaticText(Panel1, ID_STATICTEXT1, _("ARS Filter"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));    BoxSizer2->Add(StaticText1, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5);    ARSFilter = new wxTextCtrl(Panel1, ID_ARSTEXTCTRL, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_ARSTEXTCTRL"));    ARSFilter->SetToolTip(_("Enter an ARS string to filter users with"));    BoxSizer2->Add(ARSFilter, 1, wxALL|wxALIGN_LEFT|wxALIGN_TOP, 5);    ClearButton = new wxButton(Panel1, ID_CLEARBUTTON, _("Clear"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CLEARBUTTON"));    ClearButton->SetToolTip(_("Clears the ARS filter"));    BoxSizer2->Add(ClearButton, 0, wxALL|wxALIGN_LEFT|wxALIGN_TOP, 5);    BoxSizer1->Add(BoxSizer2, 0, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_TOP, 5);    UserList = new wxListCtrl(Panel1, ID_USERLISTCTRL, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_HRULES, wxDefaultValidator, _T("ID_USERLISTCTRL"));    BoxSizer1->Add(UserList, 1, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_TOP, 5);    BoxSizer3 = new wxBoxSizer(wxHORIZONTAL);    BoxSizer4 = new wxBoxSizer(wxHORIZONTAL);    StaticText2 = new wxStaticText(Panel1, ID_STATICTEXT2, _("Quick Validation Sets"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT2"));    BoxSizer4->Add(StaticText2, 0, wxALL|wxALIGN_LEFT|wxALIGN_TOP, 5);    QVChoice = new wxChoice(Panel1, ID_QVCHOICE, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_QVCHOICE"));    QVChoice->SetSelection( QVChoice->Append(_("Select a set")) );    QVChoice->Disable();    BoxSizer4->Add(QVChoice, 0, wxALL|wxALIGN_LEFT|wxALIGN_TOP, 5);    BoxSizer3->Add(BoxSizer4, 1, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_TOP, 5);    BoxSizer5 = new wxBoxSizer(wxHORIZONTAL);    RefreshButton = new wxButton(Panel1, ID_REFRESHBUTTON, _("Refresh"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_REFRESHBUTTON"));    RefreshButton->SetToolTip(_("Reloads the user database"));    BoxSizer5->Add(RefreshButton, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP, 5);    EditButton = new wxButton(Panel1, ID_EDITBUTTON, _("Edit"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_EDITBUTTON"));    EditButton->Disable();    BoxSizer5->Add(EditButton, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP, 5);    BoxSizer3->Add(BoxSizer5, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP, 5);    BoxSizer1->Add(BoxSizer3, 0, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_TOP, 5);    Panel1->SetSizer(BoxSizer1);    BoxSizer1->Fit(Panel1);    BoxSizer1->SetSizeHints(Panel1);    MenuBar1 = new wxMenuBar();    Menu1 = new wxMenu();    MenuItem1 = new wxMenuItem(Menu1, idMenuQuit, _("Quit\tAlt-F4"), _("Quit the application"), wxITEM_NORMAL);    Menu1->Append(MenuItem1);    MenuBar1->Append(Menu1, _("&File"));    Menu2 = new wxMenu();    MenuItem2 = new wxMenuItem(Menu2, idMenuAbout, _("About\tF1"), _("Show info about this application"), wxITEM_NORMAL);    Menu2->Append(MenuItem2);    MenuBar1->Append(Menu2, _("Help"));    SetMenuBar(MenuBar1);    StatusBar1 = new wxStatusBar(this, ID_STATUSBAR1, 0, _T("ID_STATUSBAR1"));    int __wxStatusBarWidths_1[1] = { -1 };    int __wxStatusBarStyles_1[1] = { wxSB_NORMAL };    StatusBar1->SetFieldsCount(1,__wxStatusBarWidths_1);    StatusBar1->SetStatusStyles(1,__wxStatusBarStyles_1);    SetStatusBar(StatusBar1);    Connect(ID_ARSTEXTCTRL,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnARSFilterText);    Connect(ID_CLEARBUTTON,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnClearButtonClick);    Connect(ID_USERLISTCTRL,wxEVT_COMMAND_LIST_ITEM_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnUserListItemSelect);    Connect(ID_USERLISTCTRL,wxEVT_COMMAND_LIST_ITEM_DESELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnUserListItemSelect);    Connect(ID_QVCHOICE,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnQVChoiceSelect);    Connect(ID_REFRESHBUTTON,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnRefreshButtonClick);    Connect(idMenuQuit,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnQuit);    Connect(idMenuAbout,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnAbout);    //*)

	// Stupid Windows Dance
	wxListItem itemCol;
	itemCol.SetText(_("Num"));
	itemCol.SetImage(-1);
	UserList->InsertColumn(0, itemCol);
	UserList->DeleteColumn(0);

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
	UserList->SetColumnWidth(0, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(1, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(2, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(3, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(4, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(5, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(6, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(7, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(8, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(9, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(10, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(11, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(12, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(13, wxLIST_AUTOSIZE);
	UserList->SetColumnWidth(14, wxLIST_AUTOSIZE);

    /*
     * Ideally this would go right after QVChoice->SetSelection
     */

	for(int i=0;i<10;i++) {
		wxString    str;
		wxString    fstr;
		char        flags[33];

		fstr=wxString::From8BitData(ltoaf(App->cfg.val_flags1[i],flags));
		str.Printf(_("%d  SL: %-2d  F1: "),i,App->cfg.val_level[i]);
		str += fstr;
		QVChoice->Append(str);
	}

	BoxSizer1->SetSizeHints(this);
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

void SBBS_User_ListFrame::OnUserListItemSelect(wxListEvent& event)
{
    if(UserList->GetSelectedItemCount())
        QVChoice->Enable();
    else
        QVChoice->Enable();
}

void SBBS_User_ListFrame::OnQVChoiceSelect(wxCommandEvent& event)
{
    user_t      user;
    int         res;
    long        item=-1;
    wxArrayInt  selections;
    int         set=QVChoice->GetSelection()-1;

    for(;;) {
        item = UserList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if(item==-1)
            break;
        user.number=UserList->GetItemData(item);
        if((res=getuserdat(&App->cfg, &user))) {
            wxString    str;
            str.Printf(_("Error %d loading user %d."),res,user.number);
            wxMessageDialog *dlg=new wxMessageDialog(NULL, str, str);
            dlg->ShowModal();
            continue;
        }
        user.flags1=App->cfg.val_flags1[set];
        user.flags2=App->cfg.val_flags2[set];
        user.flags3=App->cfg.val_flags3[set];
        user.flags4=App->cfg.val_flags4[set];
        user.exempt=App->cfg.val_exempt[set];
        user.rest=App->cfg.val_rest[set];
        if(App->cfg.val_expire[set]) {
            user.expire=time(NULL)
                +(App->cfg.val_expire[set]*24*60*60);
        }
        else
            user.expire=0;
        user.level=App->cfg.val_level[set];
        if((res=putuserdat(&App->cfg,&user))) {
            wxString    str;
            str.Printf(_("Error %d saving user %d."),res,user.number);
            wxMessageDialog *dlg=new wxMessageDialog(NULL, str, str);
            dlg->ShowModal();
            continue;
        }
    }
    fillUserList();
}
