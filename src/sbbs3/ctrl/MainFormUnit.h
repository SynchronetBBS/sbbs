/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

//---------------------------------------------------------------------------
#ifndef MainFormUnitH
#define MainFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Menus.hpp>
#include <ComCtrls.hpp>
#include <ToolWin.hpp>
#include <ExtCtrls.hpp>
#include <ActnList.hpp>
//---------------------------------------------------------------------------
#include <io.h>			// Undefined symbol '_chmod' ???
#include "scfgdefs.h"  	// scfg_t
#include "ftpsrvr.h"	// ftp_startup_t
#include "websrvr.h"	// web_startup_t
#include "mailsrvr.h"	// mail_startup_t
#include "services.h"   // services_startup_t
#include <ImgList.hpp>
#include <Buttons.hpp>
#include <Graphics.hpp>
#include <vcl\Registry.hpp>	/* TRegistry */
#include "trayicon.h"
#include "Trayicon.h"
#include <Dialogs.hpp>
//---------------------------------------------------------------------------
#define APP_TITLE "Synchronet Control Panel"
#define REG_KEY "\\Software\\Swindell\\"APP_TITLE"\\"
//---------------------------------------------------------------------------
class TMainForm : public TForm
{
__published:	// IDE-managed Components
    TMainMenu *MainMenu;
    TMenuItem *FileMenuItem;
    TMenuItem *ViewMenuItem;
    TMenuItem *ViewNodesMenuItem;
    TMenuItem *FileExitMenuItem;
	TMenuItem *ViewTelnetMenuItem;
	TMenuItem *ViewMailServerMenuItem;
	TToolBar *Toolbar;
	TToolButton *ViewTelnetButton;
	TMenuItem *ViewToolbarMenuItem;
	TMenuItem *TelnetMenuItem;
	TMenuItem *TelnetConfigureMenuItem;
	TMenuItem *TelnetStartMenuItem;
	TMenuItem *TelnetStopMenuItem;
	TActionList *ActionList;
	TAction *TelnetStart;
	TAction *TelnetStop;
	TAction *TelnetConfigure;
	TImageList *ImageList;
	TAction *NodeListStart;
	TAction *NodeListStop;
	TAction *MailStart;
	TAction *MailStop;
    TMenuItem *MailMenuItem;
    TMenuItem *MailConfigureMenuItem;
    TMenuItem *MailStartMenuItem;
    TAction *MailConfigure;
    TToolButton *ViewNodesButton;
    TToolButton *ViewMailServerButton;
    TMenuItem *MailStopMenuItem;
	TAction *ViewTelnet;
    TAction *ViewNodes;
    TAction *ViewMailServer;
	TAction *ViewFtpServer;
	TAction *FtpStart;
	TAction *FtpStop;
	TToolButton *ViewFtpServerButton;
	TMenuItem *ViewFtpServerMenuItem;
	TAction *FtpConfigure;
	TMenuItem *FtpMenuItem;
	TMenuItem *FtpConfigureMenuItem;
	TMenuItem *FtpStartMenuItem;
	TMenuItem *FtpStopMenuItem;
	TMenuItem *BBSMenuItem;
	TMenuItem *BBSConfigureMenuItem;
	TPanel *TopPanel;
	TSplitter *HorizontalSplitter;
	TPanel *BottomPanel;
	TTimer *StatsTimer;
	TAction *ViewStats;
	TToolButton *ViewStatsButton;
	TMenuItem *ViewStatsMenuItem;
	TPageControl *LowerLeftPageControl;
    TSplitter *BottomVerticalSplitter;
	TPageControl *LowerRightPageControl;
	TStatusBar *StatusBar;
	TPageControl *UpperLeftPageControl;
    TSplitter *TopVerticalSplitter;
	TPageControl *UpperRightPageControl;
	TTimer *StartupTimer;
	TMenuItem *ViewStatusBarMenuItem;
	TImage *Logo;
	TMenuItem *HelpMenuItem;
	TMenuItem *HelpAboutMenuItem;
	TMenuItem *FileSoundMenuItem;
	TToolButton *SoundButton;
	TAction *SoundToggle;
	TMenuItem *ForceNetworkCalloutMenuItem;
	TMenuItem *ForceTimedEventMenuItem;
	TMenuItem *N1;
	TMenuItem *BBSEditMenuItem;
	TMenuItem *BBSEditGuruBrain;
	TMenuItem *BBSEditTextStrings;
	TMenuItem *BBSEditColors;
	TMenuItem *BBSEditAnswer;
	TMenuItem *BBSEditSystemInfo;
	TMenuItem *BBSEditNewUserMsg;
	TMenuItem *BBSEditFeedbackMsg;
	TMenuItem *BBSEditFilters;
	TMenuItem *BBSEditNameFilter;
	TMenuItem *BBSEditIPFilter;
	TMenuItem *PhoneNumberPHONECAN1;
	TMenuItem *BBSEditFilenameFilter;
	TMenuItem *BBSEditNameFilterBadMsg;
	TMenuItem *BBSEditPhoneFilterBadMsg;
	TMenuItem *BBSEditFileFilterBadMsg;
	TMenuItem *BBSEditZipComment;
	TMenuItem *N2;
	TMenuItem *MailEditMenuItem;
	TMenuItem *MailEditAliasList;
    TTimer *UpTimer;
    TAction *ChatToggle;
    TMenuItem *SysopAvailableforChat1;
    TToolButton *SysopAvailButton;
    TAction *ViewClients;
    TToolButton *ViewClientsButton;
    TMenuItem *Clients1;
    TMenuItem *UserMenuItem;
    TMenuItem *UserEditMenuItem;
    TToolButton *EditUserButton;
    TAction *UserEdit;
    TToolButton *ToolButton1;
    TMenuItem *N3;
    TMenuItem *FtpEditMenuItem;
    TMenuItem *FtpEditAliasList;
    TMenuItem *FtpEditLoginMessage;
    TMenuItem *FtpEditHelloMessage;
    TMenuItem *FtpEditGoodbyeMessage;
    TMenuItem *N4;
    TMenuItem *FileOpenMenuItem;
    TMenuItem *N5;
    TMenuItem *BBSEditIPFilterMsg;
    TMenuItem *BBSEditHostFilter;
    TMenuItem *BBSEditHostFilterMsg;
    TMenuItem *AllowedRelayList;
	TMenuItem *SaveSettingsMenuItem;
    TMenuItem *N6;
    TMenuItem *TelnetEditMenuItem;
    TMenuItem *TelnetEditRLoginList;
    TMenuItem *BBSLoginMenuItem;
    TMenuItem *UserListMenuItem;
    TMenuItem *MailViewMenuItem;
    TMenuItem *MailViewTodaysLog;
    TMenuItem *MailViewDatesLog;
    TMenuItem *FtpViewMenuItem;
    TMenuItem *FtpViewTodaysLog;
    TMenuItem *FtpViewDatesLog;
    TMenuItem *MailViewYesterdaysLog;
    TMenuItem *FtpViewYesterdaysLog;
    TToolButton *ToolButton2;
    TMenuItem *BBSViewMenuItem;
    TMenuItem *BBSViewAnotherDaysLog;
    TMenuItem *BBSViewYesterdaysLog;
    TMenuItem *BBSViewTodaysLog;
    TMenuItem *ViewErrorLog1;
    TMenuItem *ViewStatisticsLog1;
    TAction *UserList;
    TToolButton *UserListButton;
    TMenuItem *HelpIndexMenuItem;
    TMenuItem *N7;
    TMenuItem *FilePropertiesMenuItem;
    TMenuItem *N8;
    TTrayIcon *TrayIcon;
    TAction *Properties;
    TPopupMenu *TrayPopupMenu;
    TMenuItem *RestoreTrayMenuItem;
    TMenuItem *CloseTrayMenuItem;
    TMenuItem *HelpSysopMenuItem;
    TAction *ViewEvents;
    TMenuItem *ViewEventsMenuItem;
    TMenuItem *ConfigureTrayMenuItem;
    TMenuItem *BBSEditAutoMsg;
    TMenuItem *BBSEditLogonMessage;
    TMenuItem *BBSEditNoNodesMessage;
    TMenuItem *BBSConfigWizardMenuItem;
    TMenuItem *N9;
    TMenuItem *UserEditTrayMenuItem;
    TMenuItem *BBSEditNupGuessMenuItem;
    TMenuItem *BBSEditEmailFilterMenuItem;
    TMenuItem *BBSEditBadEmailMessageMenuItem;
    TMenuItem *ViewHackAttemptLogMenuItem;
    TMenuItem *ConfigureBBSTrayMenuItem;
    TMenuItem *ConfigureTelnetTrayMenuItem;
    TMenuItem *ConfigureFtpTrayMenuItem;
    TMenuItem *ConfigureMailTrayMenuItem;
    TAction *ReloadConfig;
    TMenuItem *ReloadConfigurationFiles1;
    TToolButton *ToolButton3;
    TToolButton *ReloadConfigButton;
    TMenuItem *MailViewSpamLog;
    TMenuItem *DomainList;
    TMenuItem *BBSEditSubjectFilterMenuItem;
    TAction *ServicesStart;
    TAction *ServicesStop;
    TMenuItem *ServicesMenuItem;
    TMenuItem *ServicesConfigureMenuItem;
    TMenuItem *ServicesStartMenuItem;
    TMenuItem *ServicesStopMenuItem;
    TAction *ServicesConfigure;
    TMenuItem *UserTruncateMenuItem;
	TMenuItem *HelpFAQMenuItem;
	TAction *MailRecycle;
	TAction *FtpRecycle;
	TAction *ServicesRecycle;
	TMenuItem *Recycle1;
	TMenuItem *Recycle2;
	TMenuItem *Recycle3;
	TAction *TelnetRecycle;
	TMenuItem *DnsBlacklists;
	TMenuItem *ExportSettingsMenuItem;
	TMenuItem *ImportSettingsMenuItem;
	TMenuItem *DNSBLExemptions;
	TMenuItem *BBSEditIPFilterSilent;
	TMenuItem *ExternalMailProc;
	TOpenDialog *OpenDialog;
	TSaveDialog *SaveDialog;
	TMenuItem *BBSEditNewUserEmail;
	void __fastcall FileExitMenuItemClick(TObject *Sender);
	void __fastcall ViewToolbarMenuItemClick(TObject *Sender);
	void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
	void __fastcall FormCloseQuery(TObject *Sender, bool &CanClose);
	void __fastcall TelnetStartExecute(TObject *Sender);
	void __fastcall TelnetStopExecute(TObject *Sender);
	void __fastcall TelnetConfigureExecute(TObject *Sender);
	void __fastcall NodeListStartExecute(TObject *Sender);
	void __fastcall NodeListStopExecute(TObject *Sender);
    void __fastcall MailConfigureExecute(TObject *Sender);
    void __fastcall MailStartExecute(TObject *Sender);
    void __fastcall MailStopExecute(TObject *Sender);
    void __fastcall ViewTelnetExecute(TObject *Sender);
    void __fastcall ViewNodesExecute(TObject *Sender);
    void __fastcall ViewMailServerExecute(TObject *Sender);
	void __fastcall ViewFtpServerExecute(TObject *Sender);
	void __fastcall FtpStartExecute(TObject *Sender);
	void __fastcall FtpStopExecute(TObject *Sender);
	void __fastcall FtpConfigureExecute(TObject *Sender);
	void __fastcall CloseTimerTimer(TObject *Sender);
	void __fastcall BBSConfigureMenuItemClick(TObject *Sender);
	void __fastcall FormCreate(TObject *Sender);
	void __fastcall FormShow(TObject *Sender);
	void __fastcall NodeCloseButtonClick(TObject *Sender);
	void __fastcall TelnetCloseButtonClick(TObject *Sender);
	void __fastcall MailCloseButtonClick(TObject *Sender);
	void __fastcall FtpCloseButtonClick(TObject *Sender);
	void __fastcall StatsTimerTick(TObject *Sender);
	void __fastcall ViewStatsExecute(TObject *Sender);
	void __fastcall StatsCloseButtonClick(TObject *Sender);
	void __fastcall StartupTimerTick(TObject *Sender);
	void __fastcall ViewStatusBarMenuItemClick(TObject *Sender);
	void __fastcall HelpAboutMenuItemClick(TObject *Sender);
	void __fastcall SoundToggleExecute(TObject *Sender);
	void __fastcall BBSStatisticsLogMenuItemClick(TObject *Sender);
	void __fastcall ForceTimedEventMenuItemClick(TObject *Sender);
	void __fastcall ForceNetworkCalloutMenuItemClick(TObject *Sender);
	void __fastcall TextMenuItemEditClick(TObject *Sender);
	void __fastcall CtrlMenuItemEditClick(TObject *Sender);
    void __fastcall UpTimerTick(TObject *Sender);
    void __fastcall BBSViewErrorLogMenuItemClick(TObject *Sender);
    void __fastcall ChatToggleExecute(TObject *Sender);
    void __fastcall ViewClientsExecute(TObject *Sender);
    void __fastcall UserEditExecute(TObject *Sender);
    void __fastcall FileOpenMenuItemClick(TObject *Sender);
    void __fastcall SaveSettings(TObject *Sender);
    void __fastcall ImportSettings(TObject *Sender);
    void __fastcall ExportSettings(TObject *Sender);
    void __fastcall BBSLoginMenuItemClick(TObject *Sender);
    void __fastcall ViewLogClick(TObject *Sender);
    void __fastcall UserListExecute(TObject *Sender);
    void __fastcall WebPageMenuItemClick(TObject *Sender);
    void __fastcall TrayIconRestore(TObject *Sender);
    void __fastcall PropertiesExecute(TObject *Sender);
    void __fastcall CloseTrayMenuItemClick(TObject *Sender);
    void __fastcall RestoreTrayMenuItemClick(TObject *Sender);
    void __fastcall ViewEventsExecute(TObject *Sender);
    void __fastcall DataMenuItemClick(TObject *Sender);
    void __fastcall BBSConfigWizardMenuItemClick(TObject *Sender);
    void __fastcall PageControlUnDock(TObject *Sender,
          TControl *Client, TWinControl *NewTarget, bool &Allow);
    void __fastcall ReloadConfigExecute(TObject *Sender);
    void __fastcall ServicesStartExecute(TObject *Sender);
    void __fastcall ServicesStopExecute(TObject *Sender);
    void __fastcall ServicesConfigureExecute(TObject *Sender);
    void __fastcall UserTruncateMenuItemClick(TObject *Sender);
	void __fastcall MailRecycleExecute(TObject *Sender);
	void __fastcall FtpRecycleExecute(TObject *Sender);
	void __fastcall ServicesRecycleExecute(TObject *Sender);
	void __fastcall TelnetRecycleExecute(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TMainForm(TComponent* Owner);
    bool            Initialized;
    bool			SysAutoStart;
    bool            FtpAutoStart;
    bool            WebAutoStart;
    bool            MailAutoStart;
    bool            ServicesAutoStart;
    bool			MailLogFile;
    bool			FtpLogFile;
    AnsiString		CtrlDirectory;
    AnsiString      Hostname;
    AnsiString      LoginCommand;
    AnsiString      ConfigCommand;
    AnsiString		Password;
    DWORD			JS_MaxBytes;
    bool            MinimizeToSysTray;
    bool            UndockableForms;
    scfg_t		    cfg;
    bbs_startup_t 	bbs_startup;
    ftp_startup_t	ftp_startup;
    web_startup_t	web_startup;
    mail_startup_t 	mail_startup;
    services_startup_t  services_startup;
    int             NodeDisplayInterval;
    int             ClientDisplayInterval;
    int             SpyTerminalWidth;
    int             SpyTerminalHeight;
    TFont*          SpyTerminalFont;
    bool            SpyTerminalKeyboardActive;
	TPageControl* __fastcall PageControl(int num);
	int __fastcall  PageNum(TPageControl* obj);
    void __fastcall FormMinimize(TObject *Sender);
    TColor __fastcall ReadColor(TRegistry*, AnsiString);
    void __fastcall WriteColor(TRegistry*, AnsiString, TColor);
    void __fastcall ReadFont(AnsiString, TFont*);
    void __fastcall WriteFont(AnsiString, TFont*);
    void __fastcall FilterIP(char* ip_addr
						,char* prot, char* username);
};

//---------------------------------------------------------------------------
extern PACKAGE TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
