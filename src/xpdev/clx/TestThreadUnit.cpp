//---------------------------------------------------------------------------
#include <stdio.h>	/* printf */
#pragma hdrstop

#include "TestThreadUnit.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------
__fastcall TTestThread::TTestThread(bool CreateSuspended)
	: TThread(CreateSuspended)
{
	ChildEvent = new TSimpleEvent();
    ParentEvent = new TSimpleEvent();
}
//---------------------------------------------------------------------------
void __fastcall TTestThread::Execute()
{
	printf("TestThread::Execute() entry\n");
	ChildEvent->SetEvent();

	for(int i=0;i<10;i++) {
        ParentEvent->WaitFor(INFINITE);
    	ParentEvent->ResetEvent();
		printf(" <child>\n");
		ChildEvent->SetEvent();
	}

	printf("thread_test exit\n");
}
//---------------------------------------------------------------------------
 