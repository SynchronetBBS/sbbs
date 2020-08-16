/* $Id: PropertiesDlgUnit.h,v 1.20 2018/07/24 01:11:29 rswindell Exp $ */

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

//----------------------------------------------------------------------------
#ifndef PropertiesDlgUnitH
#define PropertiesDlgUnitH
//----------------------------------------------------------------------------
#include <vcl\ExtCtrls.hpp>
#include <vcl\Buttons.hpp>
#include <vcl\StdCtrls.hpp>
#include <vcl\Controls.hpp>
#include <vcl\Forms.hpp>
#include <vcl\Graphics.hpp>
#include <vcl\Classes.hpp>
#include <vcl\SysUtils.hpp>
#include <vcl\Windows.hpp>
#include <vcl\System.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
//----------------------------------------------------------------------------
class TPropertiesDlg : public TForm
{
__published:
	TButton *OKBtn;
	TButton *CancelBtn;
	TPageControl *PageControl;
	TTabSheet *SettingsTabSheet;
	TLabel *Label3;
	TEdit *LoginCmdEdit;
	TLabel *Label2;
	TEdit *ConfigCmdEdit;
	TLabel *Label4;
	TEdit *NodeIntEdit;
	TUpDown *NodeIntUpDown;
	TLabel *Label5;
	TEdit *ClientIntEdit;
	TUpDown *ClientIntUpDown;
	TCheckBox *TrayIconCheckBox;
	TLabel *PasswordLabel;
	TEdit *PasswordEdit;
	TTabSheet *CustomizeTabSheet;
	TComboBox *SourceComboBox;
	TEdit *ExampleEdit;
	TButton *FontButton;
	TButton *BackgroundButton;
	TFontDialog *FontDialog1;
	TButton *ApplyButton;
	TComboBox *TargetComboBox;
	TColorDialog *ColorDialog1;
        TTabSheet *AdvancedTabSheet;
        TLabel *Label1;
        TEdit *CtrlDirEdit;
        TLabel *Label6;
        TEdit *HostnameEdit;
	TLabel *Label8;
	TEdit *MaxLogLenEdit;
	TLabel *Label9;
	TEdit *TempDirEdit;
	TCheckBox *UndockableCheckBox;
	TLabel *Label10;
	TEdit *SemFreqEdit;
	TUpDown *SemFreqUpDown;
	TTabSheet *JavaScriptTabSheet;
	TLabel *Label7;
	TEdit *JS_MaxBytesEdit;
	TLabel *Label11;
	TEdit *JS_ContextStackEdit;
	TLabel *Label12;
    TEdit *JS_TimeLimitEdit;
	TLabel *Label13;
	TEdit *JS_GcIntervalEdit;
	TLabel *Label14;
	TEdit *JS_YieldIntervalEdit;
    TCheckBox *FileAssociationsCheckBox;
    TGroupBox *LogFontGroupBox;
    TComboBox *LogLevelComboBox;
    TEdit *LogFontExampleEdit;
    TButton *LogFontButton;
    TLabel *LogLevelLabel;
    TLabel *Label16;
    TEdit *JS_LoadPathEdit;
    TLabel *ErrorSoundLabel;
    TEdit *ErrorSoundEdit;
    TButton *ErrorSoundButton;
    TOpenDialog *OpenDialog;
    TTabSheet *SecurityTabSheet;
    TGroupBox *FailedLoginAttemptGroupBox;
    TLabel *LoginAttemptDelayLabel;
    TEdit *LoginAttemptDelayEdit;
    TLabel *LoginAttemptThrottleLabel;
    TEdit *LoginAttemptThrottleEdit;
    TLabel *LoginAttemptHackThreshold;
    TEdit *LoginAttemptHackThresholdEdit;
        TLabel *LoginAttemptTempBanThresholdLabel;
        TEdit *LoginAttemptTempBanThresholdEdit;
        TLabel *LoginAttemptTempBanDurationLabel;
        TEdit *LoginAttemptTempBanDurationEdit;
        TLabel *LoginAttemptFilterThresholdLabel;
        TEdit *LoginAttemptFilterThresholdEdit;
    TButton *HelpBtn;
	void __fastcall FormShow(TObject *Sender);
	void __fastcall TrayIconCheckBoxClick(TObject *Sender);
	void __fastcall SourceComboBoxChange(TObject *Sender);
	void __fastcall FontButtonClick(TObject *Sender);
	void __fastcall BackgroundButtonClick(TObject *Sender);
	void __fastcall ApplyButtonClick(TObject *Sender);
    void __fastcall LogLevelComboBoxChange(TObject *Sender);
    void __fastcall LogFontButtonClick(TObject *Sender);
    void __fastcall ErrorSoundButtonClick(TObject *Sender);
    void __fastcall HelpBtnClick(TObject *Sender);
private:
public:
	virtual __fastcall TPropertiesDlg(TComponent* AOwner);
	void __fastcall TPropertiesDlg::ChangeScheme(int target);
};
//----------------------------------------------------------------------------
extern PACKAGE TPropertiesDlg *PropertiesDlg;
//----------------------------------------------------------------------------
#endif    
