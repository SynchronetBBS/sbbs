/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: MainFormUnit.h,v 1.94 2020/04/17 20:38:56 rswindell Exp $ */

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
	TMenuItem *FileEditMenuItem;
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
    TMenuItem *BBSViewErrorLogMenuItem;
	TMenuItem *ViewStatisticsLog;
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
	TMenuItem *ViewGuruChatLogMenuItem;
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
    TMenuItem *ServicesRecycleMenuItem;
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
	TMenuItem *BBSPreviewMenuItem;
	TMenuItem *FileEditTextFiles;
	TMenuItem *FileEditConfigFiles;
	TMenuItem *FileEditJavaScript;
	TMenuItem *BBSEditBajaMenuItem;
	TMenuItem *BBSEditFile;
	TMenuItem *N10;
	TMenuItem *SpamBaitList;
	TMenuItem *SpamBlockList;
	TMenuItem *SpamBlockExemptions;
    TTimer *LogTimer;
	TTimer *ServiceStatusTimer;
    TMenuItem *ViewWebServerMenuItem;
    TAction *ViewWebServer;
    TToolButton *ViewWebServerButton;
    TAction *WebStart;
    TAction *WebStop;
    TAction *WebRecycle;
    TAction *WebConfigure;
    TMenuItem *WebMenuItem;
    TMenuItem *WebConfigureMenuItem;
    TMenuItem *WebStartMenuItem;
    TMenuItem *WebStopMenuItem;
    TMenuItem *WebRecycleMenuItem;
    TMenuItem *N11;
    TMenuItem *WebEditMenuItem;
    TMenuItem *WebEditMimeTypesMenuItem;
    TMenuItem *N12;
    TMenuItem *ServicesEditMenuItem;
    TMenuItem *ServicesEditIniMenuOption;
    TAction *ViewServices;
    TMenuItem *ViewServicesMenuItem;
    TMenuItem *BBSEditTwitList;
    TMenuItem *WebEditHandlersMenuItem;
    TMenuItem *WebEditCgiEnvMenuItem;
    TMenuItem *FtpEditBadLoginMessage;
    TMenuItem *ConfigureWebTrayMenuItem;
    TMenuItem *ConfigureServicesTrayMenuItem;
    TMenuItem *HelpTechnicalSupportMenuItem;
    TTimer *SemFileTimer;
    TMenuItem *ViewErrorLogMenuItem;
    TAction *TelnetPause;
    TMenuItem *TelnetPauseMenuItem;
    TAction *MailPause;
    TAction *WebPause;
    TAction *FtpPause;
    TAction *ServicesPause;
    TMenuItem *MailPauseMenuItem;
    TMenuItem *FtpPauseMenuItem;
    TMenuItem *WebPauseMenuItem;
    TMenuItem *ServicesPauseMenuItem;
    TMenuItem *LogoffMessage;
    TPopupMenu *StatusBarPopupMenu;
    TMenuItem *ClearErrorCounter;
    TAction *ClearErrors;
    TMenuItem *ClearErrorMenuItem;
    TMenuItem *BBSEditModOptsMenuItem;
    TAction *ViewErrorLog;
    TMenuItem *ViewErrorLogPopupMenuItem;
    TMenuItem *BBSEditPasswordFilterMenuItem;
    TMenuItem *BBSEditBadPasswordMessageMenuItem;
    TMenuItem *ViewLoginAttemptsMenuItem;
    TMenuItem *HelpDonateMenuItem;
        TPopupMenu *LogPopupMenu;
    TMenuItem *LogPopupCopyAll;
    TMenuItem *LogPopupCopy;
    TMenuItem *ViewFailedLoginsPopupMenuItem;
    TMenuItem *ClearFailedLoginsPopupMenuItem;
    TMenuItem *LogRefresh;
    TMenuItem *FidonetMenuItem;
    TMenuItem *FidonetConfigureMenuItem;
    TMenuItem *N13;
    TMenuItem *FidonetViewMenuItem;
    TMenuItem *FidonetEditMenuItem;
    TMenuItem *sbbsechoLogViewMenuItem;
    TMenuItem *binkstasViewMenuItem;
    TMenuItem *echostatsViewMenuItem;
    TMenuItem *badareasViewMenuItem;
    TMenuItem *sbbsechoEditMenuItem;
    TMenuItem *areafileEditMenuItem;
    TMenuItem *FidonetPollMenuItem;
    TMenuItem *FileRunMenuItem;
    TMenuItem *FileRunUpdateMenuItem;
    TMenuItem *FileRunChkSetupMenuItem;
    TMenuItem *FileRunInitFidonetMenuItem;
    TMenuItem *FileRunJSMenuItem;
    TMenuItem *FileRunInstallXtrnMenuItem;
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
    void __fastcall LogTimerTick(TObject *Sender);
    void __fastcall ChatToggleExecute(TObject *Sender);
    void __fastcall ViewClientsExecute(TObject *Sender);
    void __fastcall UserEditExecute(TObject *Sender);
	void __fastcall SetControls(void);
    void __fastcall SaveSettings(TObject *Sender);
    bool __fastcall SaveIniSettings(TObject* Sender);    
    void __fastcall SaveRegistrySettings(TObject* Sender);    
    void __fastcall ImportSettings(TObject *Sender);
    void __fastcall ImportFormSettings(TMemIniFile* IniFile, const char* section, TForm* form);
    void __fastcall ExportFormSettings(TMemIniFile* IniFile, const char* section, TForm* form);
    void __fastcall ImportFont(TMemIniFile* IniFile, const char* section, AnsiString prefix, TFont* Font);
    void __fastcall ExportFont(TMemIniFile* IniFile, const char* section, AnsiString prefix, TFont* Font);
    void __fastcall ExportSettings(TObject *Sender);
    void __fastcall BBSLoginMenuItemClick(TObject *Sender);
    void __fastcall ViewLogClick(TObject *Sender);
    void __fastcall RunJSClick(TObject *Sender);
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
	void __fastcall BBSPreviewMenuItemClick(TObject *Sender);
	void __fastcall FileEditTextFilesClick(TObject *Sender);
	void __fastcall BBSEditBajaMenuItemClick(TObject *Sender);
	void __fastcall FileEditJavaScriptClick(TObject *Sender);
	void __fastcall FileEditConfigFilesClick(TObject *Sender);
	void __fastcall BBSEditFileClick(TObject *Sender);
	void __fastcall ServiceStatusTimerTick(TObject *Sender);
    void __fastcall ViewWebServerExecute(TObject *Sender);
    void __fastcall WebStartExecute(TObject *Sender);
    void __fastcall WebStopExecute(TObject *Sender);
    void __fastcall WebRecycleExecute(TObject *Sender);
    void __fastcall WebConfigureExecute(TObject *Sender);
    void __fastcall ViewServicesExecute(TObject *Sender);
    void __fastcall SemFileTimerTick(TObject *Sender);
    void __fastcall ClearErrorsExecute(TObject *Sender);
    void __fastcall ViewErrorLogExecute(TObject *Sender);
    void __fastcall ViewLoginAttemptsMenuItemClick(TObject *Sender);
        void __fastcall LogPopupPauseClick(TObject *Sender);
    void __fastcall LogPopupCopyAllClick(TObject *Sender);
    void __fastcall LogPopupCopyClick(TObject *Sender);
    void __fastcall ClearFailedLoginsPopupMenuItemClick(TObject *Sender);
    void __fastcall RefreshLogClick(TObject *Sender);
    void __fastcall FidonetConfigureMenuItemClick(TObject *Sender);
    void __fastcall FidonetPollMenuItemClick(TObject *Sender);
    void __fastcall FileRunJSMenuItemClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TMainForm(TComponent* Owner);
    bool            Initialized;
    BOOL			SysAutoStart;
    BOOL            FtpAutoStart;
    BOOL            WebAutoStart;
    BOOL            MailAutoStart;
    BOOL            ServicesAutoStart;
    bool			MailLogFile;
    bool			FtpLogFile;
    AnsiString      LoginCommand;
    AnsiString      ConfigCommand;
    AnsiString		Password;
    bool            MinimizeToSysTray;
    bool            UndockableForms;
    bool            UseFileAssociations;
    scfg_t		    cfg;
    char		    ini_file[MAX_PATH+1];
    bbs_startup_t 	bbs_startup;
    ftp_startup_t	ftp_startup;
    web_startup_t	web_startup;
    mail_startup_t 	mail_startup;
    services_startup_t  services_startup;
    global_startup_t	global;
    HANDLE			bbs_log;
    HANDLE			event_log;
    HANDLE			ftp_log;
    HANDLE			web_log;
    HANDLE			mail_log;
    HANDLE			services_log;
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
    void __fastcall EditFile(AnsiString filename, AnsiString Caption="Edit");
    void __fastcall ViewFile(AnsiString filename, AnsiString Caption);
    void __fastcall reload_config(void);    
    BOOL __fastcall bbsServiceEnabled(void);
    BOOL __fastcall ftpServiceEnabled(void);
    BOOL __fastcall webServiceEnabled(void);
    BOOL __fastcall mailServiceEnabled(void);
    BOOL __fastcall servicesServiceEnabled(void);
    TFont*          LogFont[LOG_DEBUG+1];
    TFont* __fastcall LogAttributes(int log_level, TColor, TFont*);
};

extern const char* LogLevelDesc[];

//---------------------------------------------------------------------------
extern PACKAGE TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
