
//---------------------------------------------------------------------------

#include <clx.h>
#pragma hdrstop

#include "ThreadUnit.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------

//   Important: Methods and properties of objects in CLX can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall TMyThread::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------

__fastcall TMyThread::TMyThread(bool CreateSuspended)
	: TThread(CreateSuspended)
{
}
void __fastcall TMyThread::AddToLog()
{
    	Log->Lines->Add(ThreadID);
}
//---------------------------------------------------------------------------
void __fastcall TMyThread::Execute()
{
	while(!Terminated) {
    	Synchronize(AddToLog);
        /* TThreadList is a simplier way to use a thread-safe TList */
		CriticalSection->Enter();
		List->Add(new AnsiString(ThreadID));
        CriticalSection->Leave();
 	    Sleep(1000);
    }
}
//---------------------------------------------------------------------------
