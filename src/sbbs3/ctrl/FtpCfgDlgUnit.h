/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

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

//----------------------------------------------------------------------------
#ifndef FtpCfgDlgUnitH
#define FtpCfgDlgUnitH
//----------------------------------------------------------------------------
#include <vcl\System.hpp>
#include <vcl\Windows.hpp>
#include <vcl\SysUtils.hpp>
#include <vcl\Classes.hpp>
#include <vcl\Graphics.hpp>
#include <vcl\StdCtrls.hpp>
#include <vcl\Forms.hpp>
#include <vcl\Controls.hpp>
#include <vcl\Buttons.hpp>
#include <vcl\ExtCtrls.hpp>
#include <Dialogs.hpp>
#include <ComCtrls.hpp>
//----------------------------------------------------------------------------
class TFtpCfgDlg : public TForm
{
__published:
	TOpenDialog *OpenDialog;
    TPageControl *PageControl;
    TTabSheet *GeneralTabSheet;
    TTabSheet *LogTabSheet;
    TCheckBox *AutoStartCheckBox;
    TCheckBox *DebugTxCheckBox;
    TCheckBox *CmdLogCheckBox;
    TCheckBox *DebugDataCheckBox;
    TCheckBox *LogFileCheckBox;
    TTabSheet *SoundTabSheet;
    TLabel *MaxClientesLabel;
    TEdit *MaxClientsEdit;
    TLabel *MaxInactivityLabel;
    TEdit *MaxInactivityEdit;
    TLabel *PortLabel;
    TEdit *PortEdit;
    TLabel *InterfaceLabel;
    TEdit *NetworkInterfaceEdit;
    TCheckBox *AllowQWKCheckBox;
    TCheckBox *LocalFileSysCheckBox;
    TCheckBox *HostnameCheckBox;
    TButton *OKBtn;
    TButton *CancelBtn;
    TButton *ApplyBtn;
    TCheckBox *AllowBounceCheckBox;
    TTabSheet *IndexTabSheet;
    TCheckBox *AutoIndexCheckBox;
    TEdit *IndexFileNameEdit;
    TLabel *QwkTimeoutLabel;
    TEdit *QwkTimeoutEdit;
    TTabSheet *PasvTabSheet;
    TLabel *PasvIpLabel;
    TEdit *PasvIPv4AddrEdit;
    TLabel *PasvPortLabel;
    TEdit *PasvPortLowEdit;
    TEdit *PasvPortHighEdit;
    TLabel *PasvPortThroughLabel;
    TCheckBox *PasvIpLookupCheckBox;
    TLabel *MaxConConLabel;
    TEdit *MaxConConEdit;
    TButton *ConfigureSoundButton;
	void __fastcall FormShow(TObject *Sender);
	void __fastcall OKBtnClick(TObject *Sender);
	void __fastcall AutoIndexCheckBoxClick(TObject *Sender);
    void __fastcall PasvIpLookupCheckBoxClick(TObject *Sender);
    void __fastcall AllowQWKCheckBoxClick(TObject *Sender);
    void __fastcall ConfigureSoundButtonClick(TObject *Sender);
private:
public:
	virtual __fastcall TFtpCfgDlg(TComponent* AOwner);
};
//----------------------------------------------------------------------------
extern PACKAGE TFtpCfgDlg *FtpCfgDlg;
//----------------------------------------------------------------------------
#endif    
