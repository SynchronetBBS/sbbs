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
//---------------------------------------------------------------------------

#include "sbbs.h"
#include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "ConfigWizardUnit.h"
#include <stdio.h>      // sprintf
#include <winsock.h>    // addresses and such

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"

#define ILLEGAL_QWKID_CHARS  "*?" ILLEGAL_FILENAME_CHARS

char ctrl_dir[MAX_PATH+1];
short tz_val[]= {
     0
    ,AST
    ,EST
    ,CST
    ,MST
    ,PST
    ,YST
    ,HST
    ,BST
    ,MID
    ,VAN
    ,EDM
    ,WIN
    ,BOG
    ,CAR
    ,RIO
    ,FER
    ,AZO
    ,WET
    ,CET
    ,EET
    ,MOS
    ,DUB
    ,KAB
    ,KAR
    ,BOM
    ,KAT
    ,DHA
    ,BAN
    ,HON
    ,TOK
    ,ACST
    ,AEST
    ,NOU
    ,NZST
};

char* tz_str[]={
     "Universal (UTC/GMT)"
    ,"U.S. - Atlantic"
    ,"U.S. - Eastern"
    ,"U.S. - Central"
    ,"U.S. - Mountain"
    ,"U.S. - Pacific"
    ,"U.S. - Yukon"
    ,"U.S. - Hawaii/Alaska"
    ,"U.S. - Bering"
    ,"Midway"
    ,"Vancouver"
    ,"Edmonton"
    ,"Winnipeg"
    ,"Bogota"
    ,"Caracas"
    ,"Rio de Janeiro"
    ,"Fernando de Noronha"
    ,"Azores"
    ,"Western Europe (WET)"
    ,"Central Europe (CET)"
    ,"Eastern Europe (EET)"
    ,"Moscow"
    ,"Dubai"
    ,"Kabul"
    ,"Karachi"
    ,"Bombay"
    ,"Kathmandu"
    ,"Dhaka"
    ,"Bangkok"
    ,"Hong Kong"
    ,"Tokyo"
    ,"Australian Central"
    ,"Australian Eastern"
    ,"Noumea"
    ,"New Zealand"
};

//---------------------------------------------------------------------------
__fastcall TConfigWizard::TConfigWizard(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::FormShow(TObject *Sender)
{
    char str[512];
    int i;
    int status;

   	Application->BringToFront();

    memset(&scfg,0,sizeof(scfg));
    SAFECOPY(scfg.ctrl_dir,MainForm->global.ctrl_dir);
    scfg.size=sizeof(scfg);
    char error[256];
	SAFECOPY(error,UNKNOWN_LOAD_ERROR);
    if(!load_cfg(&scfg,NULL,FALSE,error,sizeof(error))) {
        Application->MessageBox(error,"ERROR Loading Configuration"
        	,MB_OK|MB_ICONEXCLAMATION);
        Close();
        return;
    }

    if(scfg.new_install) {
        TIME_ZONE_INFORMATION tz;
	    memset(&tz,0,sizeof(tz));
    	DWORD tzRet=GetTimeZoneInformation(&tz);

        /* Convert to SMB tz format */
        scfg.sys_timezone=0;
		if(tz.Bias>0) {
        	switch(tz.Bias|US_ZONE) {
				case AST:
				case EST:
				case CST:
				case MST:
				case PST:
				case YST:
				case HST:
				case BST:
	                scfg.sys_timezone=tz.Bias|US_ZONE;
			        if(tzRet==TIME_ZONE_ID_DAYLIGHT)
			   	    	scfg.sys_timezone|=DAYLIGHT;
                    break;
                default:
					scfg.sys_timezone=tz.Bias|WESTERN_ZONE;
			        if(tzRet==TIME_ZONE_ID_DAYLIGHT)
			   	    	scfg.sys_timezone|=DAYLIGHT;
                    break;
            }
		} else if(tz.Bias<0) {
            switch(tzRet) {
                case TIME_ZONE_ID_DAYLIGHT:
                    tz.Bias += tz.DaylightBias;
                    break;
                case TIME_ZONE_ID_STANDARD:
                    tz.Bias += tz.StandardBias;
                    break;
            }
			scfg.sys_timezone=(-tz.Bias)|EASTERN_ZONE;
			if(tzRet==TIME_ZONE_ID_DAYLIGHT)
			   	scfg.sys_timezone|=DAYLIGHT;
        }
#if 0
        /* Get DNS Server Address */
        str_list_t dns_list = getNameServerList();
        if(dns_list!=NULL) {
            SAFECOPY(MainForm->mail_startup.dns_server,dns_list[0]);
            freeNameServerList(dns_list);
        }
#endif        
    } else {
        SystemNameEdit->Text=AnsiString(scfg.sys_name);
        SystemLocationEdit->Text=AnsiString(scfg.sys_location);
        SysopNameEdit->Text=AnsiString(scfg.sys_op);
        SystemPasswordEdit->Text=AnsiString(scfg.sys_pass);
        InternetAddressComboBox->Text=AnsiString(scfg.sys_inetaddr);
        QWKIDEdit->Text=AnsiString(scfg.sys_id);
        QNetTaglineEdit->Text=AnsiString(scfg.qnet_tagline);
        AllNodesTelnetCheckBox->Checked
            =(scfg.sys_nodes==MainForm->bbs_startup.last_node);
    }

    NewUsersCheckBox->Checked=!(scfg.sys_misc&SM_CLOSED);
    FeedbackCheckBox->Checked=scfg.node_valuser;
    AliasesCheckBox->Checked=scfg.uq&UQ_ALIASES;
    NewUsersCheckBoxClick(Sender);

    if(scfg.sys_misc&SM_USRVDELM)
        DeletedEmailYesButton->Checked=true;
    else if(scfg.sys_misc&SM_SYSVDELM)
        DeletedEmailSysopButton->Checked=true;
    else
        DeletedEmailNoButton->Checked=true;

//    DNSAddressEdit->Text=MainForm->mail_startup.dns_server;
    MaxMailUpDown->Position=MainForm->mail_startup.max_clients;
    MaxFtpUpDown->Position=MainForm->ftp_startup.max_clients;
    MaxWebUpDown->Position=MainForm->web_startup.max_clients;
    NodesUpDown->Position=scfg.sys_nodes;

    for(i=0;i<sizeof(tz_str)/sizeof(tz_str[0]);i++) {
        char str[128];
        if(tz_val[i] && !(tz_val[i]&US_ZONE))
            sprintf(str,"%c%u:%02u %s"
                ,tz_val[i]&WESTERN_ZONE ? '-':'+'
                ,(tz_val[i]&0xfff)/60
                ,(tz_val[i]&0xfff)%60
                ,tz_str[i]
                );
        else
            SAFECOPY(str,tz_str[i]);
        TimeZoneComboBox->Items->Add(str);
    }
   	sprintf(str,"Other (%s)",smb_zonestr(scfg.sys_timezone,NULL));
    TimeZoneComboBox->Items->Add(str);

    for(i=0;i<sizeof(tz_val)/sizeof(tz_val[0]);i++)
        if((scfg.sys_timezone&((short)~DAYLIGHT))==tz_val[i])
            break;
    TimeZoneComboBox->ItemIndex=i;
    DaylightCheckBox->Enabled=scfg.sys_timezone&US_ZONE;
    DaylightCheckBox->Checked=scfg.sys_timezone&DAYLIGHT;
    if(scfg.sys_misc&SM_MILITARY)
        Time24hrRadioButton->Checked=true;
    else
        Time12hrRadioButton->Checked=true;
    if(scfg.sys_misc&SM_EURODATE)
        DateEuRadioButton->Checked=true;
    else
        DateUsRadioButton->Checked=true;

    WizNotebook->PageIndex=0;
	ProgressBar->Position=0;
    ProgressBar->Max=WizNotebook->Pages->Count-1;
    IllegalCharsLabel->Caption="Illegal characters: '"
        ILLEGAL_QWKID_CHARS "'";

	NextButton->Enabled=true;
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::NextButtonClick(TObject *Sender)
{
    if(ProgressBar->Position==ProgressBar->Max) /* Finished */
    {
        // Write Registry keys
        if(AllNodesTelnetCheckBox->Checked)
            MainForm->bbs_startup.last_node=NodesUpDown->Position;
        MainForm->ftp_startup.max_clients=MaxFtpUpDown->Position;
        MainForm->mail_startup.max_clients=MaxMailUpDown->Position;
        MainForm->web_startup.max_clients=MaxWebUpDown->Position;
//        strcpy(MainForm->mail_startup.dns_server,DNSAddressEdit->Text.c_str());

        MainForm->SaveIniSettings(Sender);

        // Write CNF files
        SAFECOPY(scfg.sys_name,SystemNameEdit->Text.c_str());
        SAFECOPY(scfg.sys_id,QWKIDEdit->Text.c_str());
        SAFECOPY(scfg.sys_location,SystemLocationEdit->Text.c_str());
        SAFECOPY(scfg.sys_op,SysopNameEdit->Text.c_str());
        SAFECOPY(scfg.sys_pass,SystemPasswordEdit->Text.c_str());
        SAFECOPY(scfg.sys_inetaddr,InternetAddressComboBox->Text.c_str());
        SAFECOPY(scfg.qnet_tagline,QNetTaglineEdit->Text.c_str());
        scfg.sys_nodes=NodesUpDown->Position;
        if(TimeZoneComboBox->ItemIndex>=0
        	&& TimeZoneComboBox->ItemIndex<=sizeof(tz_val)/sizeof(tz_val[0]))
            scfg.sys_timezone=tz_val[TimeZoneComboBox->ItemIndex];
        if(DaylightCheckBox->Checked)
            scfg.sys_timezone|=DAYLIGHT;
        if(Time24hrRadioButton->Checked)
            scfg.sys_misc|=SM_MILITARY;
        else
            scfg.sys_misc&=~SM_MILITARY;
        if(DateEuRadioButton->Checked)
            scfg.sys_misc|=SM_EURODATE;
        else
            scfg.sys_misc&=~SM_EURODATE;
        if(NewUsersCheckBox->Checked)
            scfg.sys_misc&=~SM_CLOSED;
        else
            scfg.sys_misc|=SM_CLOSED;
        if(AliasesCheckBox->Checked)
            scfg.uq|=UQ_ALIASES;
        else
            scfg.uq&=~UQ_ALIASES;
        scfg.node_valuser=FeedbackCheckBox->Checked;

        if(DeletedEmailYesButton->Checked)
            scfg.sys_misc|=(SM_USRVDELM|SM_SYSVDELM);
        else if(DeletedEmailNoButton->Checked)
            scfg.sys_misc&=~(SM_USRVDELM|SM_SYSVDELM);
        else {
            scfg.sys_misc|=SM_SYSVDELM;
            scfg.sys_misc&=~SM_USRVDELM;
        }
        scfg.new_install=FALSE;
        if(!save_cfg(&scfg,0)) {
        	Application->MessageBox("Error saving configuration"
            	,"ERROR",MB_OK|MB_ICONEXCLAMATION);
        } else
        	refresh_cfg(&scfg);
        Close();
        return;
    }
    WizNotebook->PageIndex++;
    ProgressBar->Position=WizNotebook->PageIndex;
    if(ProgressBar->Position==ProgressBar->Max) /* Last page */
        NextButton->Caption="&Finish";
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::BackButtonClick(TObject *Sender)
{
    WizNotebook->PageIndex--;
    ProgressBar->Position=WizNotebook->PageIndex;
    NextButton->Caption="&Next >";
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::CancelButtonClick(TObject *Sender)
{
    Close();
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::LogoImageClick(TObject *Sender)
{
    LogoImage->Visible=false;
    ProgressBar->Visible=false;
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::TitleLabelClick(TObject *Sender)
{
    LogoImage->Visible=true;
    ProgressBar->Visible=true;
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::WizNotebookPageChanged(TObject *Sender)
{
    TFont* current;

    switch(WizNotebook->PageIndex) {
        case 0:
            current=IntroductionLabel->Font;
            NextButton->Enabled=true;
            break;
        case 1:
            current=IdentificationLabel->Font;
//            ActiveControl=SystemNameEdit;
            if(!SysopNameEdit->Text.Length()) {
                char username[41];
                DWORD len=sizeof(username)-1;
                GetUserName(username,&len);
                SysopNameEdit->Text=AnsiString(username);
            }
            if(!SystemNameEdit->Text.Length()) {
                char bbsname[41];
                DWORD len=sizeof(bbsname)-1;
                GetComputerName(bbsname,&len);
                SystemNameEdit->Text=AnsiString(bbsname);
            }
            VerifyIdentification(Sender);
            break;
        case 2:
            current=TimeLabel->Font;
            NextButton->Enabled=true;
            break;
        case 3:
            current=InternetLabel->Font;
            if(!InternetAddressComboBox->Items->Count) {
                WSADATA WSAData;
                WSAStartup(MAKEWORD(1,1), &WSAData); /* req'd for gethostname */
                if(scfg.sys_inetaddr[0])
                    InternetAddressComboBox->Items->Add(scfg.sys_inetaddr);
                char hostname[128];
                gethostname(hostname,sizeof(hostname)-1);
                if(hostname[0]) {
                    InternetAddressComboBox->Items->Add(hostname);
                    HOSTENT* host=gethostbyname(hostname);
                    SOCKADDR_IN addr;
                    for(int i=0;host && host->h_addr_list[i];i++) {
                        addr.sin_addr.s_addr
                            =*((DWORD*)host->h_addr_list[i]);
                        InternetAddressComboBox->Items->Add(inet_ntoa(addr.sin_addr));
                    }
                }
                WSACleanup();
                InternetAddressComboBox->ItemIndex=0;
            }
            VerifyInternetAddresses(Sender);
            break;
        case 4:
            current=QWKLabel->Font;
            if(!QWKIDEdit->Text.Length()) {
                char bbsname[41];
                char qwkid[9];
                int len=0;
                SAFECOPY(bbsname,SystemNameEdit->Text.UpperCase().c_str());
                for(char*p=bbsname;*p && len<8;p++) {
                    if(strchr(ILLEGAL_QWKID_CHARS,*p))
                        continue;
                    if(!len && isdigit(*p))
                        continue;
                    if(*p<=' ')
                        continue;
                    qwkid[len++]=*p;
                }
                qwkid[len]=0;
                QWKIDEdit->Text=AnsiString(qwkid);
            }
            if(!QNetTaglineEdit->Text.Length()) {
                QNetTaglineEdit->Text = SystemNameEdit->Text +
                    " - " + InternetAddressComboBox->Text;
            }
            VerifyQWK(Sender);
            break;
        case 5:
            current=MaximumsLabel->Font;
            NextButton->Enabled=true;
            break;
        case 6:
            current=OptionsLabel->Font;
            NextButton->Enabled=true;
            break;
        case 7:
            current=VerifyLabel->Font;
            VerifyPassword(Sender);
            if(!NextButton->Enabled)
                ActiveControl=SysPassVerifyEdit;
            break;
        case 8:
            current=CompleteLabel->Font;
            break;
        default:
            return;
    }

    /* default to inactive */
    IntroductionLabel->Font->Color=clGray;
    IdentificationLabel->Font->Color=clGray;
    QWKLabel->Font->Color=clGray;
    TimeLabel->Font->Color=clGray;
    InternetLabel->Font->Color=clGray;
    MaximumsLabel->Font->Color=clGray;
    OptionsLabel->Font->Color=clGray;
    VerifyLabel->Font->Color=clGray;
    CompleteLabel->Font->Color=clGray;

    /* active */
    current->Color=clYellow;

    BackButton->Enabled=WizNotebook->PageIndex;
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::VerifyIdentification(TObject *Sender)
{
    NextButton->Enabled=(SystemNameEdit->Text.Length()
        && SystemLocationEdit->Text.Length()
        && SysopNameEdit->Text.Length()
        && SystemPasswordEdit->Text.Length()
    );
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::VerifyPassword(TObject *Sender)
{
    NextButton->Enabled=(SystemPasswordEdit->Text==SysPassVerifyEdit->Text);
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::VerifyQWK(TObject *Sender)
{
    char    qwk_id[9];

    SAFECOPY(qwk_id,QWKIDEdit->Text.c_str());
    NextButton->Enabled=(
        strlen(qwk_id)>=2
        && isalpha(qwk_id[0])
        && strcspn(qwk_id,ILLEGAL_QWKID_CHARS)==strlen(qwk_id)
        && QNetTaglineEdit->Text.Length());
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::QNetTaglineEditKeyPress(TObject *Sender,
      char &Key)
{
    if(Key==1)  /* Ctrl-A */
        QNetTaglineEdit->SetSelTextBuf("\1");
}
//---------------------------------------------------------------------------

void __fastcall TConfigWizard::VerifyInternetAddresses(TObject *Sender)
{
    NextButton->Enabled=(InternetAddressComboBox->Text.Length()
//        && DNSAddressEdit->Text.Length()
        );
}
//---------------------------------------------------------------------------
void __fastcall TConfigWizard::TimeZoneComboBoxChange(TObject *Sender)
{
    if(TimeZoneComboBox->ItemIndex>=0
    	&& TimeZoneComboBox->ItemIndex<=sizeof(tz_val)/sizeof(tz_val[0]))
        DaylightCheckBox->Enabled=tz_val[TimeZoneComboBox->ItemIndex]&US_ZONE;
    else
	    DaylightCheckBox->Enabled=false;
    if(!DaylightCheckBox->Enabled)
        DaylightCheckBox->Checked=false;
}
//---------------------------------------------------------------------------

void __fastcall TConfigWizard::NewUsersCheckBoxClick(TObject *Sender)
{
    AliasesCheckBox->Enabled=NewUsersCheckBox->Checked;
    FeedbackCheckBox->Enabled=NewUsersCheckBox->Checked;
}
//---------------------------------------------------------------------------

