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
#include <vcl.h>
#pragma hdrstop
USEFORM("MainFormUnit.cpp", MainForm);
USEFORM("CtrlPathDialogUnit.cpp", CtrlPathDialog);
USEFORM("TextFileEditUnit.cpp", TextFileEditForm);
USEFORM("TelnetFormUnit.cpp", TelnetForm);
USEFORM("FtpFormUnit.cpp", FtpForm);
USEFORM("MailFormUnit.cpp", MailForm);
USEFORM("NodeFormUnit.cpp", NodeForm);
USEFORM("StatsFormUnit.cpp", StatsForm);
USEFORM("AboutBoxFormUnit.cpp", AboutBoxForm);
USEFORM("StatsLogFormUnit.cpp", StatsLogForm);
USEFORM("CodeInputFormUnit.cpp", CodeInputForm);
USEFORM("ClientFormUnit.cpp", ClientForm);
USEFORM("SpyFormUnit.cpp", SpyForm);
USEFORM("UserListFormUnit.cpp", UserListForm);
USEFORM("UserMsgFormUnit.cpp", UserMsgForm);
USEFORM("PropertiesDlgUnit.cpp", PropertiesDlg);
USEFORM("EventsFormUnit.cpp", EventsForm);
USEFORM("ConfigWizardUnit.cpp", ConfigWizardForm);
USEFORM("ServicesFormUnit.cpp", ServicesForm);
USEFORM("TelnetCfgDlgUnit.cpp", TelnetCfgDlg);
USEFORM("MailCfgDlgUnit.cpp", MailCfgDlg);
USEFORM("FtpCfgDlgUnit.cpp", FtpCfgDlg);
USEFORM("ServicesCfgDlgUnit.cpp", ServicesCfgDlg);
USEFORM("PreviewFormUnit.cpp", PreviewForm);
USEFORM("WebFormUnit.cpp", WebForm);
USEFORM("WebCfgDlgUnit.cpp", WebCfgDlg);
USEFORM("LoginAttemptsFormUnit.cpp", LoginAttemptsForm);
USEFORM("SoundCfgDlgUnit.cpp", SoundCfgDlg);
//---------------------------------------------------------------------------
#include "MainFormUnit.h"
#include "SpyFormUnit.h"
#include "CtrlPathDialogUnit.h"
#include "sbbs_ini.h"   // sbbs_get_ini_fname
TSpyForm *SpyForms[MAX_NODES];
//---------------------------------------------------------------------------
// Large-address-safe memory manager (GitLab #1185)
//
// C++Builder 6's classic block-based memory manager is not fully large-address
// safe: it indexes its free-list with signed size arithmetic (an arithmetic-
// shift divide-by-4) and only misbehaves once the heap arena lives above
// 0x80000000.  Built /LARGEADDRESSAWARE (see build.bat) with Windows serving
// allocations top-down, the Borland arena clusters near ~0xFD000000; a high
// pointer eventually lands in a block-size header, the manager masks it to a
// ~2GB "size", walks past the end of the arena into freed address space, and
// faults - an AV deep in the Borland RTL, reached via any innocent alloc/free
// (e.g. a server's log callback appending to the GUI).
//
// Rather than give back the 4GB that LAA buys (the original #1185 mozjs185
// heap-exhaustion crash needs the headroom), replace the manager with a thin
// shim over the Windows heap, which is large-address safe by construction.
// It is installed first thing in WinMain; the previous (Borland) manager is
// chained for any block that predates the swap, so every block is freed by the
// manager that allocated it.  All post-install churn (VCL strings, the server
// log callbacks, &c.) goes to the OS heap and never touches the buggy walker.
static System::TMemoryManager BorlandMM;    // manager replaced at startup
static HANDLE                 heap_handle;  // == GetProcessHeap()

// 8-byte prefix on every block we allocate.  'self' points at the prefix
// itself, so a foreign (Borland) block would have to hold both our magic and a
// pointer to its own header at exactly this offset to be misidentified - which
// does not happen in practice, making ownership detection collision-proof.
struct heap_block {
	void*    self;
	unsigned magic;
};
#define HEAP_BLOCK_MAGIC 0x314D4253u        // "SBM1"

static void* __fastcall heap_getmem(int size)
{
	if (size < 0)
		return NULL;
	heap_block* b = (heap_block*)HeapAlloc(heap_handle, 0, (SIZE_T)(unsigned)size + sizeof(heap_block));
	if (b == NULL)
		return NULL;
	b->self  = b;
	b->magic = HEAP_BLOCK_MAGIC;
	return (char*)b + sizeof(heap_block);
}

static bool heap_owned(void* p, heap_block** blk)
{
	heap_block* b = (heap_block*)((char*)p - sizeof(heap_block));
	if (b->magic == HEAP_BLOCK_MAGIC && b->self == b) {
		*blk = b;
		return true;
	}
	return false;
}

static int __fastcall heap_freemem(void* p)
{
	if (p == NULL)
		return 0;
	heap_block* b;
	if (heap_owned(p, &b))
		return HeapFree(heap_handle, 0, b) ? 0 : -1;
	return BorlandMM.FreeMem(p);            // block allocated before we installed
}

static void* __fastcall heap_reallocmem(void* p, int size)
{
	if (p == NULL)
		return heap_getmem(size);
	if (size <= 0) {
		heap_freemem(p);
		return NULL;
	}
	heap_block* b;
	if (heap_owned(p, &b)) {
		heap_block* n = (heap_block*)HeapReAlloc(heap_handle, 0, b, (SIZE_T)(unsigned)size + sizeof(heap_block));
		if (n == NULL)
			return NULL;                    // original block left intact on failure
		n->self = n;                        // prefix may have moved
		return (char*)n + sizeof(heap_block);
	}
	return BorlandMM.ReallocMem(p, size);   // block allocated before we installed
}

static void install_heap_mem_manager(void)
{
	heap_handle = GetProcessHeap();
	System::GetMemoryManager(BorlandMM);    // save the manager we're replacing
	System::TMemoryManager mm;
	mm.GetMem     = heap_getmem;
	mm.FreeMem    = heap_freemem;
	mm.ReallocMem = heap_reallocmem;
	System::SetMemoryManager(mm);
}
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR cmd, int)
{
    install_heap_mem_manager();     // GitLab #1185: large-address-safe heap
    memset(SpyForms,0,sizeof(SpyForms));
    try
    {
        Application->Initialize();
        Application->Title = "Synchronet Control Panel";
		Application->CreateForm(__classid(TMainForm), &MainForm);
         Application->CreateForm(__classid(TTelnetForm), &TelnetForm);
         Application->CreateForm(__classid(TFtpForm), &FtpForm);
         Application->CreateForm(__classid(TWebForm), &WebForm);
         Application->CreateForm(__classid(TMailForm), &MailForm);
         Application->CreateForm(__classid(TNodeForm), &NodeForm);
         Application->CreateForm(__classid(TStatsForm), &StatsForm);
         Application->CreateForm(__classid(TClientForm), &ClientForm);
         Application->CreateForm(__classid(TUserListForm), &UserListForm);
         Application->CreateForm(__classid(TEventsForm), &EventsForm);
         Application->CreateForm(__classid(TServicesForm), &ServicesForm);
         Application->CreateForm(__classid(TLoginAttemptsForm), &LoginAttemptsForm);
         Application->CreateForm(__classid(TSoundCfgDlg), &SoundCfgDlg);
         if(cmd[0] && isdir(cmd))
            SAFECOPY(MainForm->global.ctrl_dir,cmd);
         sbbs_get_ini_fname(MainForm->ini_file, MainForm->global.ctrl_dir);
		 if(cmd[0] && fexist(cmd))
			 SAFECOPY(MainForm->ini_file, cmd);
         CreateMutex(NULL, FALSE, "sbbsctrl_running"); 	/* For use by Inno Setup */
		Application->Run();
    }
    catch (Exception &exception)
    {
             Application->ShowException(&exception);
    }
    return 0;
}
//---------------------------------------------------------------------------
