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

//----------------------------------------------------------------------------
#ifndef TelnetCfgDlgUnitH
#define TelnetCfgDlgUnitH
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
#include <Dialogs.hpp>
#include <ComCtrls.hpp>
//----------------------------------------------------------------------------
class TTelnetCfgDlg : public TForm
{
__published:
	TOpenDialog *OpenDialog;
    TPageControl *PageControl;
    TTabSheet *GeneralTabSheet;
    TTabSheet *TelnetTabSheet;
    TTabSheet *SoundTabSheet;
    TLabel *AnswerSoundLabel;
    TEdit *AnswerSoundEdit;
    TButton *AnswerSoundButton;
    TLabel *HnagupSoundLabel;
    TEdit *HangupSoundEdit;
    TButton *HangupSoundButton;
    TCheckBox *CmdLogCheckBox;
    TCheckBox *AutoStartCheckBox;
    TLabel *FirstNodeLabel;
    TEdit *FirstNodeEdit;
    TCheckBox *XtrnMinCheckBox;
    TLabel *LastNodeLabel;
    TEdit *LastNodeEdit;
    TCheckBox *HostnameCheckBox;
    TButton *OKBtn;
    TButton *CancelBtn;
    TButton *ApplyBtn;
    TLabel *InterfaceLabel;
    TEdit *TelnetInterfaceEdit;
    TLabel *TelnetPortLabel;
    TEdit *TelnetPortEdit;
	TCheckBox *TelnetGaCheckBox;
    TCheckBox *AutoLogonCheckBox;
    TTabSheet *RLoginTabSheet;
    TLabel *RLoginPortLabel;
    TEdit *RLoginPortEdit;
    TLabel *RLoginInterfaceLabel;
    TEdit *RLoginInterfaceEdit;
    TCheckBox *RLoginEnabledCheckBox;
    TButton *RLoginIPallowButton;
    TCheckBox *RLogin2ndNameCheckBox;
        TCheckBox *QWKEventsCheckBox;
    TCheckBox *JavaScriptCheckBox;
    TLabel *XtrnPollsLabel;
    TEdit *XtrnYieldEdit;
    TCheckBox *IdentityCheckBox;
	TCheckBox *EventsCheckBox;
	void __fastcall FormShow(TObject *Sender);
	void __fastcall OKBtnClick(TObject *Sender);
	void __fastcall AnswerSoundButtonClick(TObject *Sender);
	void __fastcall HangupSoundButtonClick(TObject *Sender);
    void __fastcall RLoginEnabledCheckBoxClick(TObject *Sender);
    void __fastcall RLoginIPallowButtonClick(TObject *Sender);
private:
public:
	virtual __fastcall TTelnetCfgDlg(TComponent* AOwner);
};
//----------------------------------------------------------------------------
extern PACKAGE TTelnetCfgDlg *TelnetCfgDlg;
//----------------------------------------------------------------------------
#endif    
