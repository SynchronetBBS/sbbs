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

#ifndef MainFormUnitH
#define MainFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ActnList.hpp>
#include <CheckLst.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
#include <ImgList.hpp>
#include <Menus.hpp>
#include <ToolWin.hpp>
#include "scfgdefs.h"
#include "userfields.h"
//---------------------------------------------------------------------------
class TMainForm : public TForm
{
__published:	// IDE-managed Components
    TMainMenu* MainMenu;
    TMenuItem* FileNewUserMenuItem;
    TPanel *Panel;
    TPageControl *PageControl;
    TTabSheet *PersonalTabSheet;
    TTabSheet *SecurityTabSheet;
    TTabSheet *StatsTabSheet;
    TTabSheet *SettingsTabSheet;
    TTabSheet *MsgFileSettingsTabSheet;
    TTabSheet *ExtendedCommentTabSheet;
    TScrollBar* ScrollBar;
    TMemo* Memo;
    TToolBar* ToolBar;
    TToolButton* ToolButton1;
    TButton* FindButton;
    TButton* FindNextButton;
    TStaticText* TotalStaticText;
    TLabel* Label1;
    TEdit* Status;
    TEdit* FindEdit;
    TEdit* NumberEdit;
    TEdit* AliasEdit;
    TEdit* NameEdit;
    TEdit* PhoneEdit;
    TEdit* HandleEdit;
    TEdit* ComputerEdit;
    TEdit* AddressEdit;
    TEdit* LocationEdit;
    TEdit* ZipCodeEdit;
    TEdit* ModemEdit;
    TEdit* CommentEdit;
    TEdit* PasswordEdit;
    TEdit* NetMailEdit;
    TEdit* NoteEdit;
    TEdit* SexEdit;
    TEdit* BirthDateEdit;
    TEdit* ProtocolEdit;
    TEdit* LevelEdit;
    TEdit* ExpireEdit;
    TEdit* Flags1Edit;
    TEdit* Flags2Edit;
    TEdit* Flags3Edit;
    TEdit* Flags4Edit;
    TEdit* ExemptionsEdit;
    TEdit* RestrictionsEdit;
    TEdit* CreditsEdit;
    TEdit* FreeCreditsEdit;
    TEdit* MinutesEdit;
    TEdit* FirstOnEdit;
    TEdit* LastOnEdit;
    TEdit* LogonsEdit;
    TEdit* LogonsTodayEdit;
    TEdit* TimeOnEdit;
    TEdit* LastCallTimeEdit;
    TEdit* TimeOnTodayEdit;
    TEdit* ExtraTimeEdit;
    TEdit* PostsTotalEdit;
    TEdit* PostsTodayEdit;
    TEdit* EmailTotalEdit;
    TEdit* EmailTodayEdit;
    TEdit* FeedbackEdit;
    TEdit* UploadedFilesEdit;
    TEdit* UploadedBytesEdit;
    TEdit* DownloadedFilesEdit;
    TEdit* DownloadedBytesEdit;
    TEdit* LeechEdit;
    TEdit* RowsEdit;
    TEdit* ShellEdit;
    TEdit* EditorEdit;
    TEdit* TempFileExtEdit;
    TGroupBox *TimeOnGroupBox;
    TCheckBox *ExpertCheckBox;
    TCheckListBox* TerminalCheckListBox;
    TCheckListBox* MessageCheckListBox;
    TCheckListBox* FileCheckListBox;
    TCheckListBox* LogonCheckListBox;
    TCheckListBox* QWKCheckListBox;
    TCheckListBox* ChatCheckListBox;
    TActionList *ActionList;
    TAction* SaveUser;
    TAction* NewUser;
    TImageList* ImageList;
    TGroupBox *DatesGroupBox;
    TGroupBox *LogonsGroupBoxx;
    TGroupBox *PostsGroupBox;
    TGroupBox *UploadsGroupBox;
    TGroupBox *DownloadsGroupBox;
    TGroupBox *EmailGroupBox;
    TGroupBox *FlagSetGroupBox;
    TGroupBox *TerminalGroupBox;
    TGroupBox *CommandShellGroupBox;
    TGroupBox *LogonGroupBox;
    TGroupBox *ChatGroupBox;
    TGroupBox *MessageGroupBox;
    TGroupBox *FileGroupBox;
    TGroupBox *QwkGroupBox;
    TGroupBox *AddressGroupBox;
    TStaticText *CreditsStaticText;
    TStaticText *FreeCreditsStaticText;
    TStaticText *UploadedBytesStaticText;
    TStaticText *DownloadedBytesStaticText;
    void __fastcall EditChange(TObject *Sender);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall ScrollBarChange(TObject *Sender);
    void __fastcall NumberEditKeyPress(TObject *Sender, char &Key);
    void __fastcall SaveUserExecute(TObject *Sender);
    void __fastcall NewUserExecute(TObject *Sender);
    void __fastcall FindButtonClick(TObject *Sender);
    void __fastcall FindEditKeyPress(TObject *Sender, char &Key);
    void __fastcall DeleteUserExecute(TObject *Sender);
    void __fastcall DeactivateUserExecute(TObject *Sender);
    void __fastcall FileExitMenuItemClick(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
private:	// User declarations
    int __fastcall OpenUserData(bool for_modify);
    void __fastcall GetUserText(TEdit*, AnsiString);
    void __fastcall GetUserDate(TEdit*, time_t);
    void __fastcall GetUserFlags(TEdit*, uint32_t);
    void __fastcall GetUserData(int);
    void __fastcall PutUserData(int);
    void __fastcall PutUserStr(TEdit*, enum user_field, const char*);
    void __fastcall PutUserText(TEdit*, enum user_field);
    void __fastcall PutUserDate(TEdit*, enum user_field);
    void __fastcall PutUserBytes(TEdit*, enum user_field);    
    void __fastcall SaveChanges(void);
    void __fastcall SetBit(bool set_it, uint32_t& field, uint32_t bit);
public:		// User declarations
    __fastcall TMainForm(TComponent* Owner);
    scfg_t cfg;
    user_t user;
    unsigned int users;
	char user_filename[MAX_PATH + 1];
};
//---------------------------------------------------------------------------
extern PACKAGE TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
