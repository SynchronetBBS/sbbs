//---------------------------------------------------------------------------

#include <clx.h>
#pragma hdrstop

#include "MainUnit.h"
#include "ThreadUnit.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.xfm"
TForm1 *Form1;
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
	: TForm(Owner)
{
	CriticalSection = new TCriticalSection();
    List = new TList();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::CreateButtonClick(TObject *Sender)
{
	TMyThread* NewThread = new TMyThread(true);

    NewThread->List=List;
    NewThread->Log=Log;
    NewThread->CriticalSection = CriticalSection;
    ListBox->Items->AddObject(NewThread->ThreadID, NewThread);
    NewThread->Resume();
}
//---------------------------------------------------------------------------
void __fastcall TForm1::KillButtonClick(TObject *Sender)
{
	for(int i=0;i<ListBox->Items->Count;i++)
    	if(ListBox->Selected[i]) {
        	TMyThread* MyThread = (TMyThread*)ListBox->Items->Objects[i];
            MyThread->FreeOnTerminate = true;
            MyThread->Terminate();
            ListBox->Items->Delete(i);
        }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::TimerTick(TObject *Sender)
{
	StatusBar->SimpleText = AnsiString(List->Count);
}
//---------------------------------------------------------------------------

