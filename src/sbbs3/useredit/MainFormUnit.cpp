/* Synchronet User Editor for Windows, C++ Builder */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "userdat.h"
#include "date_str.h"
#include "scfglib.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TMainForm *MainForm;
//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
    : TForm(Owner)
{
    Caption = Caption + " v" VERSION;
    ZERO_VAR(user);
    user.number = 1;
    ZERO_VAR(cfg);
    cfg.size = sizeof(cfg);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::EditChange(TObject *Sender)
{
    SaveUser->Enabled = true;
    ((TComponent*)Sender)->Tag = true; // Mark as modified
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::GetUserText(TEdit* Edit, AnsiString str)
{
    Edit->Text = str;
    Edit->Tag = false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::PutUserStr(TEdit* Edit, enum user_field fnum, const char* str)
{
    if(Edit->Tag) {
        int retval = putuserstr(&cfg, user.number, fnum, str);
        if(retval != 0) {
            char msg[128];
            char path[MAX_PATH + 1];
            SAFEPRINTF3(msg, "Error %d writing field %d to user file: %s"
                ,retval, fnum, userdat_filename(&cfg, path, sizeof(path)));
            Application->MessageBox(msg
                ,"Error"
                ,MB_OK | MB_ICONEXCLAMATION);
        } else
            Edit->Tag = false;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::PutUserText(TEdit* Edit, enum user_field fnum)
{
    PutUserStr(Edit, fnum, Edit->Text.c_str());
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::PutUserDate(TEdit* Edit, enum user_field fnum)
{
    if(Edit->Tag)
        putuserdatetime(&cfg, user.number, fnum, dstrtounix(&cfg, Edit->Text.c_str()));
    Edit->Tag = false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::GetUserDate(TEdit* Edit, time_t t)
{
    char tmp[64];
    Edit->Text = unixtodstr(&cfg, t, tmp);
    Edit->Tag = false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::GetUserFlags(TEdit* Edit, uint32_t f)
{
    char tmp[64];
    Edit->Text = u32toaf(f, tmp);
    Edit->Tag = false;
}
//---------------------------------------------------------------------------
int __fastcall TMainForm::OpenUserData(bool for_modify)
{
    int file = openuserdat(&cfg, for_modify);
	if(file < 0) {
        char msg[256];
        char path[MAX_PATH + 1];
        SAFEPRINTF2(msg, "Error %d opening user data file: %s"
            ,file, userdat_filename(&cfg, path, sizeof(path)));
        Application->MessageBox(msg, "Error" ,MB_OK | MB_ICONEXCLAMATION);
	}
    return file;
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::GetUserData(int number)
{
    char tmp[128];
	char userdat[USER_RECORD_LINE_LEN + 1];

    int file = OpenUserData(/* for_modify: */FALSE);
	if(file < 0)
        return;

    int retval = readuserdat(&cfg, number, userdat, sizeof(userdat), file, /* leave_locked: */FALSE);
    close(file);
	if(retval != 0) {
        Application->MessageBox("Error reading user file"
            ,userdat_filename(&cfg, tmp, sizeof(tmp))
            ,MB_OK | MB_ICONEXCLAMATION);
        return;
	}
    user.number = number;
	char* fields[USER_FIELD_COUNT];
	retval = parseuserdat(&cfg, userdat, &user, fields);
	if(retval != 0) {
        Application->MessageBox("Error parsing user file"
            ,userdat_filename(&cfg, tmp, sizeof(tmp))
            ,MB_OK | MB_ICONEXCLAMATION);
        return;
	}

    // Personal
    GetUserText(AliasEdit, user.alias);
    GetUserText(NameEdit, user.name);
    GetUserText(HandleEdit, user.handle);
    GetUserText(ComputerEdit, user.comp);
    GetUserText(NetMailEdit, user.netmail);
    GetUserText(AddressEdit, user.address);
    GetUserText(LocationEdit, user.location);
    GetUserText(ZipCodeEdit, user.zipcode);
    GetUserText(PhoneEdit, user.phone);
    GetUserText(ModemEdit, user.modem);
    GetUserText(CommentEdit, user.comment);
    GetUserText(PasswordEdit, user.pass);
    GetUserText(NoteEdit, user.note);
    GetUserText(BirthDateEdit, user.birth);
//    GetUserText(BirthDateEdit, format_birthdate(&cfg, user.birth, tmp, sizeof(tmp)));
    GetUserText(SexEdit, fields[USER_GENDER]);

    // Settings
    GetUserText(ProtocolEdit, fields[USER_PROT]);
    GetUserText(ShellEdit, fields[USER_SHELL]);
    GetUserText(EditorEdit, fields[USER_XEDIT]);
    GetUserText(TempFileExtEdit, user.tmpext);
    GetUserText(RowsEdit, user.rows);

    // Apply 'misc' bit-field
    ExpertCheckBox->Checked = user.misc & EXPERT;
    ExpertCheckBox->Tag = false;

    TerminalCheckListBox->Checked[0] = user.misc & AUTOTERM;
    TerminalCheckListBox->Checked[1] = !(user.misc & NO_EXASCII);
    TerminalCheckListBox->Checked[2] = user.misc & ANSI;
    TerminalCheckListBox->Checked[3] = user.misc & COLOR;
    TerminalCheckListBox->Checked[4] = user.misc & RIP;
    TerminalCheckListBox->Checked[5] = user.misc & WIP;
    TerminalCheckListBox->Checked[6] = user.misc & UPAUSE;
    TerminalCheckListBox->Checked[7] = !(user.misc & COLDKEYS);
    TerminalCheckListBox->Checked[8] = user.misc & SPIN;
    TerminalCheckListBox->Tag = false;

    MessageCheckListBox->Checked[0] = user.misc & NETMAIL;
    MessageCheckListBox->Checked[1] = user.misc & CLRSCRN;
    MessageCheckListBox->Tag = false;

    FileCheckListBox->Checked[0] = user.misc & ANFSCAN;
    FileCheckListBox->Checked[1] = user.misc & EXTDESC;
    FileCheckListBox->Checked[2] = user.misc & BATCHFLAG;
    FileCheckListBox->Checked[3] = user.misc & AUTOHANG;
    FileCheckListBox->Tag = false;

    LogonCheckListBox->Checked[0] = user.misc & ASK_NSCAN;
    LogonCheckListBox->Checked[1] = user.misc & ASK_SSCAN;
    LogonCheckListBox->Checked[2] = user.misc & CURSUB;
    LogonCheckListBox->Checked[3] = user.misc & QUIET;
    LogonCheckListBox->Checked[4] = user.misc & AUTOLOGON;
    LogonCheckListBox->Tag = false;

    // Appy 'QWK' bit-field
    QWKCheckListBox->Checked[0] = user.qwk & QWK_FILES;
    QWKCheckListBox->Checked[1] = user.qwk & QWK_EMAIL;
    QWKCheckListBox->Checked[2] = user.qwk & QWK_ALLMAIL;
    QWKCheckListBox->Checked[3] = user.qwk & QWK_DELMAIL;
    QWKCheckListBox->Checked[4] = user.qwk & QWK_BYSELF;
    QWKCheckListBox->Checked[5] = user.qwk & QWK_EXPCTLA;
    QWKCheckListBox->Checked[6] = !(user.qwk & QWK_RETCTLA);
    QWKCheckListBox->Checked[7] = user.qwk & QWK_ATTACH;
    QWKCheckListBox->Checked[8] = user.qwk & QWK_NOINDEX;
    QWKCheckListBox->Checked[9] = user.qwk & QWK_TZ;
    QWKCheckListBox->Checked[10] = user.qwk & QWK_VIA;
    QWKCheckListBox->Checked[11] = !(user.qwk & QWK_NOCTRL);
    QWKCheckListBox->Tag = false;

    // Apply 'chat' bit-field
    ChatCheckListBox->Checked[0] = user.chat & CHAT_ECHO;
    ChatCheckListBox->Checked[1] = user.chat & CHAT_ACTION;
    ChatCheckListBox->Checked[2] = !(user.chat & CHAT_NOPAGE);
    ChatCheckListBox->Checked[3] = !(user.chat & CHAT_NOACT);
    ChatCheckListBox->Checked[4] = user.chat & CHAT_SPLITP;
    ChatCheckListBox->Tag = false;

    if(user.misc & DELETED) {
        Status->Text = "Deleted User";
        Status->Color = clRed;
    } else if(user.misc & INACTIVE) {
        Status->Text = "Inactive User";
        Status->Color = clYellow;
    } else {
        Status->Text = "Active Uesr";
        Status->Color = clMenu;
    }

    GetUserText(LevelEdit, user.level);
    GetUserDate(ExpireEdit, user.expire);
    GetUserFlags(Flags1Edit, user.flags1);
    GetUserFlags(Flags2Edit, user.flags2);
    GetUserFlags(Flags3Edit, user.flags3);
    GetUserFlags(Flags4Edit, user.flags4);
    GetUserFlags(ExemptionsEdit, user.exempt);
    GetUserFlags(RestrictionsEdit, user.rest);

    GetUserText(CreditsEdit, user.cdt);
    GetUserText(FreeCreditsEdit, user.freecdt);
    GetUserText(MinutesEdit, user.min);

    // Stats
    GetUserDate(FirstOnEdit, user.firston);
    GetUserDate(LastOnEdit, user.laston);
    GetUserText(LogonsEdit, user.logons);
    GetUserText(LogonsTodayEdit, user.ltoday);
    GetUserText(TimeOnEdit, user.timeon);
    GetUserText(LastCallTimeEdit, user.tlast);
    GetUserText(TimeOnTodayEdit, user.ttoday);
    GetUserText(ExtraTimeEdit, user.textra);
    GetUserText(PostsTotalEdit, user.posts);
    GetUserText(PostsTodayEdit, user.ptoday);
    GetUserText(EmailTotalEdit, user.emails);
    GetUserText(EmailTodayEdit, user.etoday);
    GetUserText(FeedbackEdit, user.fbacks);
    GetUserText(UploadedFilesEdit, user.uls);
    GetUserText(UploadedBytesEdit, user.ulb);
    GetUserText(DownloadedFilesEdit, user.dls);
    GetUserText(DownloadedBytesEdit, user.dlb);
    GetUserText(LeechEdit, user.leech);

    // Extended Comment
    Memo->Lines->Clear();
    char path[MAX_PATH + 1];
    SAFEPRINTF2(path, "%suser/%04u.msg", cfg.data_dir, user.number);
    if(fexist(path))
        Memo->Lines->LoadFromFile(path);
    Memo->Tag = false;

    // Update Uer Number
    NumberEdit->Text = ScrollBar->Position;
    SaveUser->Enabled = false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SetBit(bool set_it, uint32_t& field, uint32_t bit)
{
    if(set_it)
        field |= bit;
    else
        field &= ~bit;
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::PutUserData(int number)
{
    if(AliasEdit->Tag) {
        int retval;
        if(user.misc & DELETED)
            retval = putusername(&cfg, number, "");
        else
            retval = putusername(&cfg, number, AliasEdit->Text.c_str());
    	if(retval != 0) {
            char tmp[MAX_PATH + 1];
            Application->MessageBox("Error writing to name index"
                ,userdat_filename(&cfg, tmp, sizeof(tmp))
                ,MB_OK | MB_ICONEXCLAMATION);
            return;
    	}
    }

    PutUserText(AliasEdit, USER_ALIAS);
    PutUserText(NameEdit, USER_NAME);
    PutUserText(HandleEdit, USER_HANDLE);
    PutUserText(ComputerEdit, USER_HOST);
    PutUserText(NetMailEdit, USER_NETMAIL);
    PutUserText(AddressEdit, USER_ADDRESS);
    PutUserText(LocationEdit, USER_LOCATION);
    PutUserText(NoteEdit, USER_NOTE);
    PutUserText(ZipCodeEdit, USER_ZIPCODE);
    PutUserText(PasswordEdit, USER_PASS);
    PutUserText(PhoneEdit, USER_PHONE);
    PutUserText(BirthDateEdit, USER_BIRTH); // TODO
    PutUserText(ModemEdit, USER_CONNECTION);
    PutUserText(SexEdit, USER_GENDER);
    PutUserText(CommentEdit, USER_COMMENT);

    PutUserText(RowsEdit, USER_ROWS);
    PutUserText(ShellEdit, USER_SHELL);
    PutUserText(EditorEdit, USER_XEDIT);
    PutUserText(ProtocolEdit, USER_PROT);
    PutUserText(TempFileExtEdit, USER_TMPEXT);

    if(ExpertCheckBox->Tag
        || LogonCheckListBox->Tag
        || TerminalCheckListBox->Tag
        || MessageCheckListBox->Tag
        || FileCheckListBox->Tag) {
        SetBit(ExpertCheckBox->Checked, user.misc, EXPERT);

        SetBit( TerminalCheckListBox->Checked[0], user.misc, AUTOTERM);
        SetBit(!TerminalCheckListBox->Checked[1], user.misc, NO_EXASCII);
        SetBit( TerminalCheckListBox->Checked[2], user.misc, ANSI);
        SetBit( TerminalCheckListBox->Checked[3], user.misc, COLOR);
        SetBit( TerminalCheckListBox->Checked[4], user.misc, RIP);
        SetBit( TerminalCheckListBox->Checked[5], user.misc, WIP);
        SetBit( TerminalCheckListBox->Checked[6], user.misc, UPAUSE);
        SetBit(!TerminalCheckListBox->Checked[7], user.misc, COLDKEYS);
        SetBit( TerminalCheckListBox->Checked[8], user.misc, SPIN);

        SetBit(MessageCheckListBox->Checked[0], user.misc, NETMAIL);
        SetBit(MessageCheckListBox->Checked[1], user.misc, CLRSCRN);

        SetBit(FileCheckListBox->Checked[0], user.misc, ANFSCAN);
        SetBit(FileCheckListBox->Checked[1], user.misc, EXTDESC);
        SetBit(FileCheckListBox->Checked[2], user.misc, BATCHFLAG);
        SetBit(FileCheckListBox->Checked[3], user.misc, AUTOHANG);

        SetBit(LogonCheckListBox->Checked[0], user.misc, ASK_NSCAN);
        SetBit(LogonCheckListBox->Checked[1], user.misc, ASK_SSCAN);
        SetBit(LogonCheckListBox->Checked[2], user.misc, CURSUB);
        SetBit(LogonCheckListBox->Checked[3], user.misc, QUIET);
        SetBit(LogonCheckListBox->Checked[4], user.misc, AUTOLOGON);

        putusermisc(&cfg, user.number, user.misc);
    }

    if(ChatCheckListBox->Tag) {
        SetBit( ChatCheckListBox->Checked[0], user.chat, CHAT_ECHO);
        SetBit( ChatCheckListBox->Checked[1], user.chat, CHAT_ACTION);
        SetBit(!ChatCheckListBox->Checked[2], user.chat, CHAT_NOPAGE);
        SetBit(!ChatCheckListBox->Checked[3], user.chat, CHAT_NOACT);
        SetBit( ChatCheckListBox->Checked[4], user.chat, CHAT_SPLITP);

        putuserchat(&cfg, user.number, user.chat);
    }

    if(QWKCheckListBox->Tag) {
        SetBit( QWKCheckListBox->Checked[0], user.qwk, QWK_FILES);
        SetBit( QWKCheckListBox->Checked[1], user.qwk, QWK_EMAIL);
        SetBit( QWKCheckListBox->Checked[2], user.qwk, QWK_ALLMAIL);
        SetBit( QWKCheckListBox->Checked[3], user.qwk, QWK_DELMAIL);
        SetBit( QWKCheckListBox->Checked[4], user.qwk, QWK_BYSELF);
        SetBit( QWKCheckListBox->Checked[5], user.qwk, QWK_EXPCTLA);
        SetBit(!QWKCheckListBox->Checked[6], user.qwk, QWK_RETCTLA);
        SetBit( QWKCheckListBox->Checked[7], user.qwk, QWK_ATTACH);
        SetBit( QWKCheckListBox->Checked[8], user.qwk, QWK_NOINDEX);
        SetBit( QWKCheckListBox->Checked[9], user.qwk, QWK_TZ);
        SetBit( QWKCheckListBox->Checked[10], user.qwk, QWK_VIA);
        SetBit(!QWKCheckListBox->Checked[11], user.qwk, QWK_NOCTRL);

        putuserqwk(&cfg, user.number, user.qwk);
    }

    PutUserText(LevelEdit, USER_LEVEL);
    PutUserText(Flags1Edit, USER_FLAGS1);
    PutUserText(Flags2Edit, USER_FLAGS2);
    PutUserText(Flags3Edit, USER_FLAGS3);
    PutUserText(Flags4Edit, USER_FLAGS4);
    PutUserText(ExemptionsEdit, USER_EXEMPT);
    PutUserText(RestrictionsEdit, USER_REST);
    PutUserDate(ExpireEdit, USER_EXPIRE);

    PutUserText(CreditsEdit, USER_CDT);
    PutUserText(FreeCreditsEdit, USER_FREECDT);
    PutUserText(MinutesEdit, USER_MIN);

    PutUserDate(FirstOnEdit, USER_FIRSTON);
    PutUserDate(LastOnEdit, USER_LASTON);
    PutUserText(LogonsEdit, USER_LOGONS);
    PutUserText(LogonsTodayEdit, USER_LTODAY);
    PutUserText(TimeOnEdit, USER_TIMEON);
    PutUserText(TimeOnTodayEdit, USER_TTODAY);
    PutUserText(LastCallTimeEdit, USER_TLAST);
    PutUserText(ExtraTimeEdit, USER_TEXTRA);
    PutUserText(PostsTotalEdit, USER_POSTS);
    PutUserText(PostsTodayEdit, USER_PTODAY);
    PutUserText(EmailTotalEdit, USER_EMAILS);
    PutUserText(EmailTodayEdit, USER_ETODAY);
    PutUserText(FeedbackEdit, USER_FBACKS);
    PutUserText(UploadedFilesEdit, USER_ULS);
    PutUserText(UploadedBytesEdit, USER_ULB);
    PutUserText(DownloadedFilesEdit, USER_DLS);
    PutUserText(DownloadedBytesEdit, USER_DLB);
    PutUserText(LeechEdit, USER_LEECH);

    if(Memo->Tag) {
        char path[MAX_PATH + 1];
        SAFEPRINTF2(path, "%suser/%04u.msg", cfg.data_dir, user.number);
        if(Memo->Lines->Count)
            Memo->Lines->SaveToFile(path);
        else
            remove(path);
    }
    GetUserData(ScrollBar->Position);
    SaveUser->Enabled = false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SaveChanges(void)
{
    if(Application->MessageBox("Save Changes", "User Modified", MB_YESNO) == IDYES)
        SaveUserExecute(MainForm);
    else
        GetUserData(ScrollBar->Position);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormShow(TObject *Sender)
{
    users = lastuser(&cfg);
    if(users < 1)
        users = 1;
    ScrollBar->Min = 1;
    ScrollBar->Max = users;
    TotalStaticText->Caption = "of " + AnsiString(users);

    AliasEdit->MaxLength = LEN_ALIAS;
    NameEdit->MaxLength = LEN_NAME;
    PhoneEdit->MaxLength = LEN_PHONE;
    HandleEdit->MaxLength = LEN_HANDLE;
    ComputerEdit->MaxLength = LEN_COMP;
    NetMailEdit->MaxLength = LEN_NETMAIL;
    NoteEdit->MaxLength = LEN_NOTE;
    AddressEdit->MaxLength = LEN_ADDRESS;
    LocationEdit->MaxLength = LEN_LOCATION;
    ZipCodeEdit->MaxLength = LEN_ZIPCODE;
    ModemEdit->MaxLength = LEN_MODEM;
    CommentEdit->MaxLength = LEN_COMMENT;
    PasswordEdit->MaxLength = LEN_PASS;
    SexEdit->MaxLength = 1;

    ScrollBar->Position = user.number;
    GetUserData(ScrollBar->Position);

    PageControl->ActivePage = PersonalTabSheet;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ScrollBarChange(TObject *Sender)
{
    if(SaveUser->Enabled)
        SaveChanges();
    users = lastuser(&cfg);
    ScrollBar->Max = users;
    TotalStaticText->Caption = "of " + AnsiString(users);
    GetUserData(ScrollBar->Position);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::NumberEditKeyPress(TObject *Sender, char &Key)
{
    if(Key != '\r')
        return;
    users = lastuser(&cfg);
    user.number = NumberEdit->Text.ToIntDef(0);
    if(user.number < 1 || (unsigned)user.number > users)
        NumberEdit->Text = ScrollBar->Position;
    else {
        ScrollBar->Position = user.number;
        GetUserData(user.number);
        Key = 0;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SaveUserExecute(TObject *Sender)
{
    PutUserData(NumberEdit->Text.ToIntDef(ScrollBar->Position));
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::NewUserExecute(TObject *Sender)
{
    if(SaveUser->Enabled)
        SaveChanges();

    ZERO_VAR(user);
    SAFECOPY(user.alias, "New User");
    newuserdefaults(&cfg, &user);

    // Create the new record
    newuserdat(&cfg, &user);

    // Set scroll bar and usernumber text
    ScrollBar->Max = lastuser(&cfg);
    ScrollBar->Position = user.number;
    NumberEdit->Text = user.number;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::DeleteUserExecute(TObject *Sender)
{
    user.misc ^= DELETED;
    putusermisc(&cfg, ScrollBar->Position, user.misc);
    LogonCheckListBox->Tag = true;  // flag as modified
    AliasEdit->Tag = true;
    PutUserData(ScrollBar->Position);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::DeactivateUserExecute(TObject *Sender)
{
    user.misc ^= INACTIVE;
    putusermisc(&cfg, ScrollBar->Position, user.misc);
    GetUserData(ScrollBar->Position);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FindButtonClick(TObject *Sender)
{
    int file = OpenUserData(/* for_modify: */false);
	if(file < 0) {
        return;
	}
    int found = 0;
    int usernumber = 1;
    if(Sender == FindNextButton)
        usernumber = ScrollBar->Position + 1;
    Screen->Cursor = crHourGlass;
    for(;;usernumber++) {
        char userdat[USER_RECORD_LINE_LEN];
        int rd = readuserdat(&cfg, usernumber, userdat, sizeof(userdat), file, /* locked: */false);
        if(rd != 0)
            break;
        TERMINATE(userdat);
        if(strcasestr(userdat, FindEdit->Text.c_str()) != NULL) {
            found = usernumber;
            break;
        }
    }
    close(file);
    Screen->Cursor = crDefault;
    if(found > 0)
        ScrollBar->Position = found;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FindEditKeyPress(TObject *Sender, char &Key)
{
    if(Key == '\r') {
        FindButtonClick(Sender);
        Key = 0;
    }
}

void __fastcall TMainForm::FileExitMenuItemClick(TObject *Sender)
{
    Close();    
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormClose(TObject *Sender, TCloseAction &Action)
{
    if(SaveUser->Enabled)
        SaveChanges();
}
//---------------------------------------------------------------------------

