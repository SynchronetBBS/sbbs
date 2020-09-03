/* $Id: WebCfgDlgUnit.h,v 1.5 2019/01/12 23:48:32 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html		    *
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

#ifndef WebCfgDlgUnitH
#define WebCfgDlgUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
//---------------------------------------------------------------------------
class TWebCfgDlg : public TForm
{
__published:	// IDE-managed Components
    TPageControl *PageControl;
    TTabSheet *GeneralTabSheet;
    TLabel *MaxClientesLabel;
    TLabel *MaxInactivityLabel;
    TLabel *PortLabel;
    TLabel *InterfaceLabel;
    TCheckBox *AutoStartCheckBox;
    TEdit *MaxClientsEdit;
    TEdit *MaxInactivityEdit;
    TEdit *PortEdit;
    TEdit *NetworkInterfaceEdit;
    TCheckBox *HostnameCheckBox;
    TTabSheet *HttpTabSheet;
    TLabel *HtmlDirLabel;
    TEdit *HtmlRootEdit;
    TEdit *ErrorSubDirEdit;
    TEdit *ServerSideJsExtEdit;
    TTabSheet *LogTabSheet;
    TCheckBox *DebugTxCheckBox;
    TCheckBox *DebugRxCheckBox;
    TCheckBox *AccessLogCheckBox;
    TTabSheet *SoundTabSheet;
    TLabel *AnswerSoundLabel;
    TLabel *HangupSoundLabel;
    TLabel *HackAttemptSoundLabel;
    TEdit *AnswerSoundEdit;
    TButton *AnswerSoundButton;
    TEdit *HangupSoundEdit;
    TButton *HangupSoundButton;
    TEdit *HackAttemptSoundEdit;
    TButton *HackAttemptSoundButton;
    TButton *OKBtn;
    TButton *CancelBtn;
    TButton *ApplyBtn;
    TLabel *ErrorSubDirLabel;
    TLabel *ServerSideJsExtLabel;
    TCheckBox *VirtualHostsCheckBox;
    TEdit *LogBaseNameEdit;
    TLabel *LogBaseLabel;
    TOpenDialog *OpenDialog;
    TLabel *IndexLabel;
    TEdit *IndexFileEdit;
    TTabSheet *CGITabSheet;
    TLabel *CGIDirLabel;
    TEdit *CGIDirEdit;
    TCheckBox *CGICheckBox;
    TLabel *CGIExtLabel;
    TEdit *CGIExtEdit;
    TLabel *CGIMaxInactivityLabel;
    TEdit *CGIMaxInactivityEdit;
    TLabel *CGIContentLabel;
    TEdit *CGIContentEdit;
    TButton *CGIEnvButton;
    TButton *WebHandlersButton;
    TTabSheet *TlsTabSheet;
    TCheckBox *TlsEnableCheckBox;
    TEdit *TlsInterfaceEdit;
    TLabel *TlsInterfaceLabel;
    TLabel *TlsPortLabel;
    TEdit *TlsPortEdit;
    TLabel *AuthTypesLabel;
    TEdit *AuthTypesEdit;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall AnswerSoundButtonClick(TObject *Sender);
    void __fastcall HangupSoundButtonClick(TObject *Sender);
    void __fastcall HackAttemptSoundButtonClick(TObject *Sender);
    void __fastcall OKBtnClick(TObject *Sender);
    void __fastcall AccessLogCheckBoxClick(TObject *Sender);
    void __fastcall CGIEnvButtonClick(TObject *Sender);
    void __fastcall WebHandlersButtonClick(TObject *Sender);
    void __fastcall CGICheckBoxClick(TObject *Sender);
    void __fastcall TlsEnableCheckBoxClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TWebCfgDlg(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TWebCfgDlg *WebCfgDlg;
//---------------------------------------------------------------------------
#endif
