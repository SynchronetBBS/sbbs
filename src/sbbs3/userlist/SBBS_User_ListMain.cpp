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
#include <wx/clipbrd.h>
#include <wx/dataobj.h>

//(*InternalHeaders(SBBS_User_ListFrame)
#include <wx/bitmap.h>
#include <wx/icon.h>
#include <wx/intl.h>
#include <wx/image.h>
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
const long SBBS_User_ListFrame::ID_QVCHOICE = wxNewId();
const long SBBS_User_ListFrame::ID_REFRESHBUTTON = wxNewId();
const long SBBS_User_ListFrame::ID_EDITBUTTON = wxNewId();
const long SBBS_User_ListFrame::ID_PANEL1 = wxNewId();
const long SBBS_User_ListFrame::ID_EDITUSER = wxNewId();
const long SBBS_User_ListFrame::ID_COPY = wxNewId();
const long SBBS_User_ListFrame::ID_COPYALL = wxNewId();
const long SBBS_User_ListFrame::ID_REFRESH = wxNewId();
//*)

struct sortData {
	int			sort;
	wxListCtrl	*UserList;
};

BEGIN_EVENT_TABLE(SBBS_User_ListFrame,wxFrame)
    //(*EventTable(SBBS_User_ListFrame)
    //*)
END_EVENT_TABLE()

int wxCALLBACK SortCallBack(wxIntPtr item1_data, wxIntPtr item2_data, wxIntPtr data)
{
	struct sortData	*sd=(struct sortData *)data;
	long			item1=sd->UserList->FindItem(-1, item1_data);
	long			item2=sd->UserList->FindItem(-1, item2_data);
	wxString		val1,val2;
	wxListItem		li;
	long			v1, v2;
	int				ret;

	li.m_itemId = (sd->sort & 0x100) ? item2 : item1;
	li.m_col = sd->sort & 0xff;
	li.m_mask = wxLIST_MASK_TEXT;
	if(!sd->UserList->GetItem(li))
		return 0;
	val1 = li.m_text;

	li.m_itemId = (sd->sort & 0x100) ? item1 : item2;
	li.m_mask = wxLIST_MASK_TEXT;
	if(!sd->UserList->GetItem(li))
		return 0;
	val2 = li.m_text;

	switch(sd->sort & 0xff) {
		// Numbers:
		case 0:
		case 3:
		case 4:
		case 12:
			if(!val1.ToLong(&v1))
				return 0;
			if(!val2.ToLong(&v2))
				return 0;
			ret = v1-v2;
			break;
		// Dates:
		case 13:
		case 14:
			v1=dstrtounix(&App->cfg, val1.mb_str(wxConvUTF8));
			v2=dstrtounix(&App->cfg, val2.mb_str(wxConvUTF8));
			ret = v1-v2;
			break;
		// Strings
		default:
			ret=val1.CmpNoCase(val2);
	}
	if(ret==0)
		return item1_data-item2_data;
	return ret;
}

void SBBS_User_ListFrame::fillUserList(void)
{
    int         totalusers=lastuser(&App->cfg);
    int         i;
    user_t      user;
    int         item=0;
    wxString    buf;
    char        datebuf[9];
    long        topitem=UserList->GetTopItem();
	struct sortData sd;

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
	sd.sort=sort;
	sd.UserList=UserList;
	UserList->SortItems(SortCallBack, (wxIntPtr)&sd);
    UserList->EnsureVisible(item);
    UserList->EnsureVisible(topitem);
	UserList->Thaw();
}

void SBBS_User_ListFrame::applyARS(void)
{
    static uchar    *last_ars=NULL;
    static ushort   last_ars_count=0;
    ushort          count;

    ars=arstr(&count, ARSFilter->GetValue().mb_str(wxConvUTF8), &App->cfg);
    if(count != last_ars_count || memcmp(last_ars, ars, count)) {
        if(last_ars != nular)
            FREE_AND_NULL(last_ars);
        last_ars=ars;
        last_ars_count=count;
        fillUserList();
    }
}

SBBS_User_ListFrame::SBBS_User_ListFrame(wxWindow* parent,wxWindowID id)
{
    //(*Initialize(SBBS_User_ListFrame)
    wxBoxSizer* BoxSizer4;
    wxBoxSizer* BoxSizer5;
    wxBoxSizer* BoxSizer2;
    wxBoxSizer* BoxSizer1;
    wxBoxSizer* BoxSizer3;

    Create(parent, id, _("SBBS User List"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, _T("id"));
    {
    wxIcon FrameIcon;
    FrameIcon.CopyFromBitmap(wxBitmap(wxImage(_T("../../conio/syncicon64.ico"))));
    SetIcon(FrameIcon);
    }
    Panel1 = new wxPanel(this, ID_PANEL1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("ID_PANEL1"));
    BoxSizer1 = new wxBoxSizer(wxVERTICAL);
    BoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
    StaticText1 = new wxStaticText(Panel1, ID_STATICTEXT1, _("ARS Filter"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
    BoxSizer2->Add(StaticText1, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5);
    ARSFilter = new wxTextCtrl(Panel1, ID_ARSTEXTCTRL, _("ACTIVE NOT DELETED"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_ARSTEXTCTRL"));
    ARSFilter->SetToolTip(_("Enter an ARS string to filter users with"));
    BoxSizer2->Add(ARSFilter, 1, wxALL|wxALIGN_LEFT|wxALIGN_TOP, 5);
    ClearButton = new wxButton(Panel1, ID_CLEARBUTTON, _("Clear"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CLEARBUTTON"));
    ClearButton->SetToolTip(_("Clears the ARS filter"));
    BoxSizer2->Add(ClearButton, 0, wxALL|wxALIGN_LEFT|wxALIGN_TOP, 5);
    BoxSizer1->Add(BoxSizer2, 0, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_TOP, 5);
    UserList = new wxListCtrl(Panel1, ID_USERLISTCTRL, wxDefaultPosition, wxDLG_UNIT(Panel1,wxSize(-1,128)), wxLC_REPORT|wxLC_HRULES, wxDefaultValidator, _T("ID_USERLISTCTRL"));
    BoxSizer1->Add(UserList, 1, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_TOP, 5);
    BoxSizer3 = new wxBoxSizer(wxHORIZONTAL);
    BoxSizer4 = new wxBoxSizer(wxHORIZONTAL);
    StaticText2 = new wxStaticText(Panel1, ID_STATICTEXT2, _("Quick Validation Sets"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT2"));
    BoxSizer4->Add(StaticText2, 0, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
    QVChoice = new wxChoice(Panel1, ID_QVCHOICE, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_QVCHOICE"));
    QVChoice->SetSelection( QVChoice->Append(_("Select a set")) );
    QVChoice->Disable();
    BoxSizer4->Add(QVChoice, 0, wxALL|wxALIGN_LEFT|wxALIGN_TOP, 5);
    BoxSizer3->Add(BoxSizer4, 1, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_TOP, 5);
    BoxSizer5 = new wxBoxSizer(wxHORIZONTAL);
    RefreshButton = new wxButton(Panel1, ID_REFRESHBUTTON, _("Refresh"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_REFRESHBUTTON"));
    RefreshButton->SetToolTip(_("Reloads the user database"));
    BoxSizer5->Add(RefreshButton, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP, 5);
    EditButton = new wxButton(Panel1, ID_EDITBUTTON, _("Edit"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_EDITBUTTON"));
    EditButton->Disable();
    BoxSizer5->Add(EditButton, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP, 5);
    BoxSizer3->Add(BoxSizer5, 0, wxALL|wxALIGN_RIGHT|wxALIGN_TOP, 5);
    BoxSizer1->Add(BoxSizer3, 0, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_TOP, 5);
    Panel1->SetSizer(BoxSizer1);
    BoxSizer1->Fit(Panel1);
    BoxSizer1->SetSizeHints(Panel1);
    MenuItem3 = new wxMenuItem((&ContextMenu), ID_EDITUSER, _("Edit User"), wxEmptyString, wxITEM_NORMAL);
    ContextMenu.Append(MenuItem3);
    MenuItem3->Enable(false);
    MenuItem4 = new wxMenuItem((&ContextMenu), ID_COPY, _("Copy"), wxEmptyString, wxITEM_NORMAL);
    ContextMenu.Append(MenuItem4);
    MenuItem5 = new wxMenuItem((&ContextMenu), ID_COPYALL, _("Copy All"), wxEmptyString, wxITEM_NORMAL);
    ContextMenu.Append(MenuItem5);
    MenuItem6 = new wxMenuItem((&ContextMenu), ID_REFRESH, _("Refresh"), wxEmptyString, wxITEM_NORMAL);
    ContextMenu.Append(MenuItem6);

    Connect(ID_ARSTEXTCTRL,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnARSFilterText);
    Connect(ID_CLEARBUTTON,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnClearButtonClick);
    Connect(ID_USERLISTCTRL,wxEVT_COMMAND_LIST_ITEM_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnUserListItemSelect);
    Connect(ID_USERLISTCTRL,wxEVT_COMMAND_LIST_ITEM_DESELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnUserListItemSelect);
    Connect(ID_USERLISTCTRL,wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK,(wxObjectEventFunction)&SBBS_User_ListFrame::OnUserListItemRClick);
    Connect(ID_USERLISTCTRL,wxEVT_COMMAND_LIST_COL_CLICK,(wxObjectEventFunction)&SBBS_User_ListFrame::OnUserListColumnClick);
    Connect(ID_QVCHOICE,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnQVChoiceSelect);
    Connect(ID_REFRESHBUTTON,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnRefreshButtonClick);
    Connect(ID_COPY,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::CopyMenuItemSelected);
    Connect(ID_COPYALL,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::CopyAllMenuItemSelected);
    Connect(ID_REFRESH,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&SBBS_User_ListFrame::OnRefreshButtonClick);
    //*)

	this->ars=NULL;
	UserList->InsertColumn(0, wxString(_("Num \x25BC")));
	this->sort=0;
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
    applyARS();
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
	this->SetName(this->GetTitle());
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
    if(!ARSFilter->IsModified())
        return;
	applyARS();
}

void SBBS_User_ListFrame::OnClearButtonClick(wxCommandEvent& event)
{
    ARSFilter->SetValue(_(""));
    applyARS();
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

	if(set<0)
		return;

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

void SBBS_User_ListFrame::OnUserListItemRClick(wxListEvent& event)
{
	PopupMenu(&ContextMenu);
}

void SBBS_User_ListFrame::CopyItems(int state)
{
	long		item=UserList->GetNextItem(-1, wxLIST_NEXT_ALL, state);
	int			cols=UserList->GetColumnCount();
	wxString	cp;
	wxListItem	li;

	for(;;) {
		for(int i=0; i<cols; i++) {
			li.m_mask = wxLIST_MASK_TEXT;
			li.m_itemId = item;
			li.m_col = i;
			if(UserList->GetItem(li))
				cp += li.m_text;
			if(i<(cols-1))
				cp += _T("\t");
		}
		item=UserList->GetNextItem(item, wxLIST_NEXT_ALL, state);
		if(item==-1)
			break;
		else
			cp += _T("\r\n");
	}

	if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(cp));
        wxTheClipboard->Close();
    }
}

void SBBS_User_ListFrame::CopyMenuItemSelected(wxCommandEvent& event)
{
	CopyItems(wxLIST_STATE_SELECTED);
}

void SBBS_User_ListFrame::CopyAllMenuItemSelected(wxCommandEvent& event)
{
	CopyItems(wxLIST_STATE_DONTCARE);
}

void SBBS_User_ListFrame::OnUserListColumnClick(wxListEvent& event)
{
	struct sortData sd;
	wxListItem		li;
	int				column=event.GetColumn();

	sd.UserList=UserList;

	UserList->Freeze();
	// First, remove char from old sort column
	li.m_mask = wxLIST_MASK_TEXT;
	UserList->GetColumn(sort & 0xff, li);
	li.m_text = li.m_text.Left(li.m_text.Length()-2);
	UserList->SetColumn(sort & 0xff, li);
	// Now, set new sort
	if((sort & 0xff)==column)
		sort ^= 0x100;
	else
		sort=column;
	sd.sort=sort;
	/* Add char to new sort column */
	li.m_mask = wxLIST_MASK_TEXT;
	UserList->GetColumn(sort & 0xff, li);
	if(sort & 0x100)
		li.m_text += _T(" \x25b2");
	else
		li.m_text += _T(" \x25bc");
	UserList->SetColumn(sort & 0xff, li);

	UserList->SortItems(SortCallBack, (wxIntPtr)&sd);
	UserList->Thaw();
}
