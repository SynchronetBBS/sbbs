//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "SpyFormUnit.h"
#include "sbbsdefs.h"

#define SPYBUF_LEN  10000
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Emulvt"
#pragma link "Emulvt"
#pragma resource "*.dfm"
TSpyForm *SpyForm;
//---------------------------------------------------------------------------
__fastcall TSpyForm::TSpyForm(TComponent* Owner)
    : TForm(Owner)
{
    Width=MainForm->SpyTerminalWidth;
    Height=MainForm->SpyTerminalHeight;
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
#if 0
        buf[rd]=0;
        if(strip_ansi(buf))
            Log->Lines->Clear();
        Log->SelLength=0;
        Log->SelStart=Log->Lines->Text.Length();
        Log->SelText=AnsiString(buf);
#else
        Terminal->WriteBuffer(buf,rd);
#endif
        Timer->Interval=100;
    } else
        Timer->Interval=500;
}
//---------------------------------------------------------------------------
void __fastcall TSpyForm::FormShow(TObject *Sender)
{
    Terminal->Font=MainForm->SpyTerminalFont;
    if((*spybuf=(RingBuf*)malloc(sizeof(RingBuf)))==NULL) {
        Terminal->WriteStr("Malloc failure!");
        return;
    }
    RingBufInit(*spybuf,SPYBUF_LEN);
    Timer->Enabled=true;
}
//---------------------------------------------------------------------------

void __fastcall TSpyForm::FormClose(TObject *Sender, TCloseAction &Action)
{
    Timer->Enabled=false;
    if(*spybuf!=NULL) {
        RingBufDispose(*spybuf);
        free(*spybuf);
        *spybuf=NULL;
    }
    MainForm->SpyTerminalWidth=Width;
    MainForm->SpyTerminalHeight=Height;
}
//---------------------------------------------------------------------------

void __fastcall TSpyForm::FontMenuItemClick(TObject *Sender)
{
	TFontDialog *FontDialog=new TFontDialog(this);

    FontDialog->Font=MainForm->SpyTerminalFont;
    FontDialog->Execute();
    MainForm->SpyTerminalFont->Assign(FontDialog->Font);
    Terminal->Font=MainForm->SpyTerminalFont;
    delete FontDialog;

}
//---------------------------------------------------------------------------

