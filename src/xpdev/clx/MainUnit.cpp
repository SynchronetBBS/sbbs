//---------------------------------------------------------------------------

#include <stdio.h>	/* getchar */
#include <clx.h>
#include "TestThreadUnit.h"
#pragma hdrstop

//---------------------------------------------------------------------------

#pragma argsused
int main(int argc, char* argv[])
{
	TTestThread* Thread=new TTestThread(true /* create suspended? */);
    Thread->Resume();
    Thread->ChildEvent->WaitFor(INFINITE);	/* wait for thread to begin */
    Thread->ChildEvent->ResetEvent();
    for(int i=0;i<10;i++) {
        printf("<parent>");
        Thread->ParentEvent->SetEvent();
        Thread->ChildEvent->WaitFor(INFINITE);
        Thread->ChildEvent->ResetEvent();
    }
    Thread->WaitFor();	/* wait for thread to end */
	return 0;
}
//---------------------------------------------------------------------------
 