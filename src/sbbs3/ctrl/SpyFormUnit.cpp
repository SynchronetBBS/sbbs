//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "SpyFormUnit.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TSpyForm *SpyForm;
//---------------------------------------------------------------------------
__fastcall TSpyForm::TSpyForm(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TSpyForm::SpyTimerTick(TObject *Sender)
{
    char    buf[1024];
    int     rd;

    if(*spybuf==NULL)
        return;

    rd=RingBufRead(*spybuf,buf,sizeof(buf)-1);
    if(rd) {
        buf[rd]=0;
        Log->SelText=AnsiString(buf);
        Timer->Interval=100;
    } else
        Timer->Interval=500;
}
//---------------------------------------------------------------------------
void __fastcall TSpyForm::FormDestroy(TObject *Sender)
{
    if(*spybuf!=NULL) {
        RingBufDispose(*spybuf);
        free(*spybuf);
        *spybuf=NULL;
    }
    Timer->Enabled=false;
}
//---------------------------------------------------------------------------
void __fastcall TSpyForm::FormShow(TObject *Sender)
{
    if((*spybuf=(RingBuf*)malloc(sizeof(RingBuf)))==NULL) {
        Log->Lines->Add("Malloc failure!");
        return;
    }
    RingBufInit(*spybuf,10000);
    Log->Lines->Add("Spying...");
    Timer->Enabled=true;
}
//---------------------------------------------------------------------------
