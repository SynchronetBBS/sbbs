//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "SpyFormUnit.h"
#include "sbbsdefs.h"

#define SPYBUF_LEN  10000
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TSpyForm *SpyForm;
//---------------------------------------------------------------------------
__fastcall TSpyForm::TSpyForm(TComponent* Owner)
    : TForm(Owner)
{
}
bool strip_ansi(char* str)
{
    char*   p=str;
    char    newstr[SPYBUF_LEN];
    int     newlen=0;
    bool    ff=false;

    for(p=str;*p && newlen<(sizeof(newstr)-1);) {

        switch(*p) {
            case BEL:   /* bell */
                Beep();
                break;
            case BS:    /* backspace */
                if(newlen)
                    newlen--;
                break;
             case FF:   /* form feed */
                newlen=0;
                ff=true;
                break;
             case ESC:
                if(*(p+1)=='[') {    /* ANSI */
                    p+=2;
                    if(!strncmp(p,"2J",2)) {
                        newlen=0;
                        ff=true;
                    }
                    while(*p && !isalpha(*p)) p++;
                }
                break;
            case CR:
                if(*(p+1)!=LF) {
                    while(newlen) {
                        newlen--;
                        if(newstr[newlen]==LF) {
                            newlen++;
                            break;
                        }
                    }
                    break;
                } /* Fall through */
             default:
                newstr[newlen++]=*p;
                break;
        }
        if(*p==0)
            break;
        p++;
    }
    newstr[newlen]=0;
    strcpy(str,newstr);
    return(ff);
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
        if(strip_ansi(buf))
            Log->Lines->Clear();
        Log->SelLength=0;
        Log->SelStart=Log->Lines->Text.Length();
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
    RingBufInit(*spybuf,SPYBUF_LEN);
    Timer->Enabled=true;
}
//---------------------------------------------------------------------------
