/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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
    char	str[256];
	char	status[128];
    int		i,n,rd,digits=1;
    node_t	node;

	if(!Visible)
		return;

    if(nodedab<1) {
    	nodedab=_sopen(AnsiString(MainForm->CtrlDirectory+"NODE.DAB").c_str()
        	,O_RDONLY|O_BINARY|O_CREAT, SH_DENYNONE, S_IREAD|S_IWRITE);
		if(nodedab==-1) {
		    ListBox->Items->Clear();
        	ListBox->Items->Add("Error "+AnsiString(errno)+" opening NODE.DAB");
            return;
        }
    }
	if(MainForm->cfg.sys_nodes>9)
		digits++;
	if(MainForm->cfg.sys_nodes>99)
		digits++;
	if(MainForm->cfg.sys_nodes>999)
		digits++;
    for(n=0;n<MainForm->cfg.sys_nodes;n++) {
	    lseek(nodedab, n*sizeof(node_t), SEEK_SET);
        if(eof(nodedab))
        	break;
#ifdef USE_LOCKING
        if(locking(nodedab, LK_NBLCK, sizeof(node_t))!=0)
        	continue;
#endif
		memset(&node,0,sizeof(node_t));
        rd=read(nodedab,&node, sizeof(node_t));
#ifdef USE_LOCKING
        lseek(nodedab, n*sizeof(node_t), SEEK_SET);
        locking(nodedab, LK_UNLCK, sizeof(node_t));
#endif

        if(rd!=sizeof(node_t))
        	continue;
            
		sprintf(str,"%*d %s"
			,digits
			,n+1
			,nodestatus(&MainForm->cfg,&node,status,sizeof(status)));
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

