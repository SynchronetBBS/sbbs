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
#include <vcl.h>
#pragma hdrstop
#include <io.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/locking.h>
#include <fcntl.h>
#include <share.h>
#include "NodeFormUnit.h"
#include "UserMsgFormUnit.h"
#include "SpyFormUnit.h"
#include "sbbs.h"
#include "nodedefs.h"
#include "userdat.h"
#include "ringbuf.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TNodeForm *NodeForm;
//---------------------------------------------------------------------------
__fastcall TNodeForm::TNodeForm(TComponent* Owner)
        : TForm(Owner)
{
//    OutputDebugString("NodeForm constructor\n");
	MainForm=(TMainForm*)Application->MainForm;
    MainForm->bbs_startup.node_spybuf
        =(RingBuf**)calloc(1,sizeof(RingBuf*)*MAX_NODES);
}
//---------------------------------------------------------------------------


void __fastcall TNodeForm::FormHide(TObject *Sender)
{
	MainForm->ViewNodesMenuItem->Checked=false;
    MainForm->ViewNodesButton->Down=false;
}
//---------------------------------------------------------------------------




void __fastcall TNodeForm::FormShow(TObject *Sender)
{
//    OutputDebugString("NodeForm::FormShow\n");

	MainForm->ViewNodesMenuItem->Checked=true;
    MainForm->ViewNodesButton->Down=true;
}
//---------------------------------------------------------------------------
int __fastcall TNodeForm::getnodedat(int node_num, node_t* node, int* file)
{
    char    errmsg[128];
    int     err;

    if((err=::getnodedat(&MainForm->cfg,node_num,node,file))!=0) {
        sprintf(errmsg,"getnodedat returned %d",err);
    	Application->MessageBox(errmsg
            ,"ERROR",MB_OK|MB_ICONEXCLAMATION);
    }

    return(err);
}
//---------------------------------------------------------------------------
int __fastcall TNodeForm::putnodedat(int node_num, node_t* node, int file)
{
    char    errmsg[128];
    int     err;

    if((err=::putnodedat(&MainForm->cfg,node_num,node, file))!=0) {
        sprintf(errmsg,"putnodedat(%d) returned %d",file,err);
    	Application->MessageBox(errmsg
            ,"ERROR",MB_OK|MB_ICONEXCLAMATION);
    }

    return(err);
}
//---------------------------------------------------------------------------

char* username(int usernumber,char *strin)
{
    char str[256];
    char c;
    int file;

    if(usernumber<1) {
        strcpy(strin,"UNKNOWN USER");
        return(strin);
    }
    sprintf(str,"%suser/name.dat",MainForm->cfg.data_dir);
    if((file=_sopen(str,O_RDONLY,SH_DENYWR,S_IWRITE))==-1) {
        return("<!ERROR opening name.dat>");
    }
    if(filelength(file)<(long)((long)usernumber*(LEN_ALIAS+2))) {
        close(file);
        strcpy(strin,"UNKNOWN USER");
        return(strin);
    }
    lseek(file,(long)((long)(usernumber-1)*(LEN_ALIAS+2)),SEEK_SET);
    read(file,strin,LEN_ALIAS);
    close(file);
    for(c=0;c<LEN_ALIAS;c++)
        if(strin[c]==ETX) break;
    strin[c]=0;
    if(!c)
        strcpy(strin,"DELETED USER");
    return(strin);
}

void __fastcall TNodeForm::TimerTick(TObject *Sender)
{
	static int nodedab;
    char	str[128],tmp[128];
    char*   mer;
    int		i,n,rd,hour;
    node_t	node;

    if(nodedab<1) {
    	nodedab=_sopen(AnsiString(MainForm->CtrlDirectory+"NODE.DAB").c_str()
        	,O_RDONLY|O_BINARY|O_CREAT, SH_DENYNONE, S_IREAD|S_IWRITE);
		if(nodedab==-1) {
		    ListBox->Items->Clear();
        	ListBox->Items->Add("Error "+AnsiString(errno)+" opening NODE.DAB");
            return;
        }
    }
    for(n=0;n<MainForm->cfg.sys_nodes;n++) {
	    lseek(nodedab, n*sizeof(node_t), SEEK_SET);
        if(eof(nodedab))
        	break;
        if(locking(nodedab, LK_NBLCK, sizeof(node_t))!=0)
        	continue;

        rd=read(nodedab,&node, sizeof(node_t));
        lseek(nodedab, n*sizeof(node_t), SEEK_SET);
        locking(nodedab, LK_UNLCK, sizeof(node_t));

        if(rd!=sizeof(node_t))
        	continue;
            
		sprintf(str,"%3d ",n+1);
        switch(node.status) {
            case NODE_WFC:
                strcat(str,"Waiting for call");
                break;
            case NODE_OFFLINE:
                strcat(str,"Offline");
                break;
            case NODE_NETTING:
                strcat(str,"Networking");
                break;
            case NODE_LOGON:
                strcat(str,"At logon prompt");
                break;
            case NODE_EVENT_WAITING:
                strcat(str,"Waiting for all nodes to become inactive");
                break;
            case NODE_EVENT_LIMBO:
                sprintf(tmp,"Waiting for node %d to finish external event"
                	,node.aux);
                strcat(str,tmp);
                break;
            case NODE_EVENT_RUNNING:
                strcat(str,"Running external event");
                break;
            case NODE_NEWUSER:
                strcat(str,"New user");
                strcat(str," applying for access ");
                if(!node.connection)
                    strcat(str,"Locally");
                else if(node.connection==0xffff) {
                	strcat(str,"via telnet");
                } else {
                    sprintf(tmp,"at %ubps",node.connection);
                    strcat(str,tmp);
                }
                break;
            case NODE_QUIET:
            case NODE_INUSE:
//                sprintf(tmp, "User #%d ",node.useron);
                username(node.useron,tmp);
                strcat(str,tmp);
                strcat(str," ");
                switch(node.action) {
                    case NODE_MAIN:
                        strcat(str,"at main menu");
                        break;
                    case NODE_RMSG:
                        strcat(str,"reading messages");
                        break;
                    case NODE_RMAL:
                        strcat(str,"reading mail");
                        break;
                    case NODE_RSML:
                        strcat(str,"reading sent mail");
                        break;
                    case NODE_RTXT:
                        strcat(str,"reading text files");
                        break;
                    case NODE_PMSG:
                        strcat(str,"posting message");
                        break;
                    case NODE_SMAL:
                        strcat(str,"sending mail");
                        break;
                    case NODE_AMSG:
                        strcat(str,"posting auto-message");
                        break;
                    case NODE_XTRN:
                        if(!node.aux)
                            strcat(str,"at external program menu");
                        else if(node.aux<=MainForm->cfg.total_xtrns) {
                          	sprintf(tmp,"running %s"
 	                           ,MainForm->cfg.xtrn[node.aux-1]->name);
                            strcat(str,tmp);
                        } else {
                            sprintf(tmp,"running external program #%d"
                            	,node.aux);
                            strcat(str,tmp);
                        }
                        break;
                    case NODE_DFLT:
                        strcat(str,"changing defaults");
                        break;
                    case NODE_XFER:
                        strcat(str,"at transfer menu");
                        break;
                    case NODE_RFSD:
                        sprintf(tmp,"retrieving from device #%d",node.aux);
                        strcat(str,tmp);
                        break;
                    case NODE_DLNG:
                        strcat(str,"downloading");
                        break;
                    case NODE_ULNG:
                        strcat(str,"uploading");
                        break;
                    case NODE_BXFR:
                        strcat(str,"transferring bidirectional");
                        break;
                    case NODE_LFIL:
                        strcat(str,"listing files");
                        break;
                    case NODE_LOGN:
                        strcat(str,"logging on");
                        break;
                    case NODE_LCHT:
                        strcat(str,"in local chat with sysop");
                        break;
                    case NODE_MCHT:
                        if(node.aux) {
                            sprintf(tmp,"in multinode chat channel %d"
                            	,node.aux&0xff);
                            if(node.aux&0x1f00) { /* password */
                                strcat(tmp,"* ");
//                                strcat(tmp, unpackchatpass(tmp,node));
                            }
                            strcat(str,tmp);
                        }
                        else
                            strcat(str,"in multinode global chat channel");
                        break;
                    case NODE_PAGE:
                        sprintf(tmp,"paging node %u for private chat",node.aux);
                        strcat(str,tmp);
                        break;
                    case NODE_PCHT:
                        if(!node.aux)
                            strcat(str,"in local chat with sysop");
                        else {
                            sprintf(tmp,"in private chat with node %u"
                                ,node.aux);
                            strcat(str,tmp);
                        }
                        break;
                    case NODE_GCHT:
                        strcat(str,"chatting with The Guru");
                        break;
                    case NODE_CHAT:
                        strcat(str,"in chat section");
                        break;
                    case NODE_TQWK:
                        strcat(str,"transferring QWK packet");
                        break;
                    case NODE_SYSP:
                        strcat(str,"performing sysop activities");
                        break;
                    default:
                        strcat(str,itoa(node.action,tmp,10));
                        break;  }
                if(!node.connection)
                    strcat(str," locally");
                if(node.connection==0xffff) {
                	strcat(str," via telnet");
                } else {
                    sprintf(tmp," at %ubps",node.connection);
                    strcat(str,tmp);
                }
                if(node.action==NODE_DLNG) {
                    if((node.aux/60)>=12) {
                        if(node.aux/60==12)
                            hour=12;
                        else
                            hour=(node.aux/60)-12;
                        mer="pm";
                    } else {
                        if((node.aux/60)==0)    /* 12 midnite */
                            hour=12;
                        else hour=node.aux/60;
                        mer="am";
                    }
                    sprintf(tmp, " ETA %02d:%02d %s"
                        ,hour,node.aux-((node.aux/60)*60),mer);
                    strcat(str, tmp); }
                break; }
        if(node.misc&(NODE_LOCK|NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG)) {
            strcat(str," (");
            if(node.misc&NODE_AOFF)
                strcat(str,"A");
            if(node.misc&NODE_LOCK)
                strcat(str,"L");
            if(node.misc&(NODE_MSGW|NODE_NMSG))
                strcat(str,"M");
            if(node.misc&NODE_POFF)
                strcat(str,"P");
            strcat(str,")"); }
        if(((node.misc
            &(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN))
            || node.status==NODE_QUIET)) {
            strcat(str," [");
            if(node.misc&NODE_ANON)
                strcat(str,"A");
            if(node.misc&NODE_INTR)
                strcat(str,"I");
            if(node.misc&NODE_RRUN)
                strcat(str,"R");
            if(node.misc&NODE_UDAT)
                strcat(str,"U");
            if(node.status==NODE_QUIET)
                strcat(str,"Q");
            if(node.misc&NODE_EVENT)
                strcat(str,"E");
            if(node.misc&NODE_DOWN)
                strcat(str,"D");
            if(node.misc&NODE_LCHAT)
                strcat(str,"C");
            strcat(str,"]"); }
        if(node.errors) {
            sprintf(tmp, " %d error%c",node.errors, node.errors>1 ? 's' : '\0' );
            strcat(str,tmp);
        }
        AnsiString Str=AnsiString(str);
        if(ListBox->Items->Count<n+1)
        	ListBox->Items->Add(Str);
		else if(ListBox->Items->Strings[n]!=Str) {
            bool selected=ListBox->Selected[n]; // save selected state
        	ListBox->Items->Strings[n]=str;
            ListBox->Selected[n]=selected;      // restore
        }
    }
    if(n!=MainForm->cfg.sys_nodes) {    /* read error or something */
        close(nodedab);
        nodedab=-1;
    }
    Timer->Enabled=true;
}
//---------------------------------------------------------------------------



void __fastcall TNodeForm::InterruptNodeButtonClick(TObject *Sender)
{
	int i;
    int file;
    node_t node;

    for(i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]==true) {
        	if(getnodedat(i+1,&node,&file))
                break;
            node.misc^=NODE_INTR;
            if(putnodedat(i+1,&node,file))
                break;
        }
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::LockNodeButtonClick(TObject *Sender)
{
	int i;
    int file;
    node_t node;

    for(i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]==true) {
        	if(getnodedat(i+1,&node,&file))
                break;
            node.misc^=NODE_LOCK;
            if(putnodedat(i+1,&node,file))
                break;
        }
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::RerunNodeButtonClick(TObject *Sender)
{
	int i;
    int file;
    node_t node;

    for(i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]==true) {
        	if(getnodedat(i+1,&node,&file))
                break;
            node.misc^=NODE_RRUN;
            if(putnodedat(i+1,&node,file))
                break;
        }
    TimerTick(Sender);
}

//---------------------------------------------------------------------------


void __fastcall TNodeForm::SelectAllMenuItemClick(TObject *Sender)
{
	int i;

   	for(i=0;i<ListBox->Items->Count;i++)
		ListBox->Selected[i]=true;

}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::DownButtonClick(TObject *Sender)
{
	int i;
    int file;
    node_t node;

    for(i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]==true) {
        	if(getnodedat(i+1,&node,&file))
                break;
            if(node.status==NODE_WFC)
            	node.status=NODE_OFFLINE;
            else if(node.status==NODE_OFFLINE)
            	node.status=NODE_WFC;
            else
	            node.misc^=NODE_DOWN;
            if(putnodedat(i+1,&node,file))
                break;
        }
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::ClearErrorButtonClick(TObject *Sender)
{
	int i;
    int file;
    node_t node;

    for(i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]==true) {
        	if(getnodedat(i+1,&node,&file))
                break;
            node.errors=0;
            if(putnodedat(i+1,&node,file))
                break;
        }
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::ChatButtonClick(TObject *Sender)
{
    char    str[512];
    char    name[128];
    int     i;
    node_t  node;

    for(i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]==true) {
        	if(getnodedat(i+1,&node,NULL))
                break;
            if(node.status==NODE_WFC || node.status>NODE_QUIET)
                continue;
            sprintf(str,"%sCHAT %s %s %d %s"
                ,MainForm->cfg.exec_dir
                ,MainForm->cfg.ctrl_dir
                ,MainForm->cfg.node_path[i]
                ,i+1
                ,username(node.useron,name)
                );
            WinExec(str,SW_SHOWNORMAL);
        }
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::SpyButtonClick(TObject *Sender)
{
    int i;

    for(i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]==true) {
            if(SpyForms[i]==NULL) {
                Application->CreateForm(__classid(TSpyForm), &SpyForms[i]);
                SpyForms[i]->inbuf=&MainForm->bbs_startup.node_inbuf[i];
                SpyForms[i]->outbuf=&MainForm->bbs_startup.node_spybuf[i];
                SpyForms[i]->Caption="Node "+AnsiString(i+1);
            }
            SpyForms[i]->Show();
        }
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::UserEditButtonClick(TObject *Sender)
{
    int     i;
    char    str[128];
    node_t  node;

    for(i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]==true) {
        	if(getnodedat(i+1,&node,NULL))
                break;
            if(node.useron==0)
                continue;
            sprintf(str,"%sUSEREDIT %s %d"
                ,MainForm->cfg.exec_dir
                ,MainForm->cfg.data_dir
                ,node.useron);
            WinExec(str,SW_SHOWNORMAL);
        }
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::UserMsgButtonClick(TObject *Sender)
{
    int     i;
    node_t  node;

	Application->CreateForm(__classid(TUserMsgForm), &UserMsgForm);
    UserMsgForm->Memo->Text="\1n\1y\1hMessage From Sysop:\1w ";
    UserMsgForm->Memo->SelStart=UserMsgForm->Memo->Text.Length();
	if(UserMsgForm->ShowModal()==mrOk) {
        UserMsgForm->Memo->Lines->Add("");
        for(i=0;i<ListBox->Items->Count;i++)
            if(ListBox->Selected[i]==true) {
               	if(getnodedat(i+1,&node,NULL))
                    break;
                if(node.useron==0)
                    continue;
                putsmsg(&MainForm->cfg,node.useron
                    ,UserMsgForm->Memo->Text.c_str());
            }
    }
    delete UserMsgForm;
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TNodeForm::RefreshMenuItemClick(TObject *Sender)
{
    ListBox->Clear();
    TimerTick(Sender);
}
//---------------------------------------------------------------------------

