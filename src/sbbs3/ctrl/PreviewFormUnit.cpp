//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "PreviewFormUnit.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TPreviewForm *PreviewForm;
//---------------------------------------------------------------------------
__fastcall TPreviewForm::TPreviewForm(TComponent* Owner)
	: TForm(Owner)
{
    Width=MainForm->SpyTerminalWidth;
    Height=MainForm->SpyTerminalHeight;
    Terminal = new TEmulVT(this);
    Terminal->Parent=this;
    Terminal->Align=alClient;
    Terminal->Rows=MAX_ROW;
    Terminal->Font=MainForm->SpyTerminalFont;
    Terminal->AutoWrap=true;
    ActiveControl=Terminal;
}
//---------------------------------------------------------------------------
__fastcall TPreviewForm::~TPreviewForm()
{
	delete Terminal;
}

#define ANSI_ESC "\x1b["

//---------------------------------------------------------------------------
void __fastcall TPreviewForm::FormShow(TObject *Sender)
{
	char str[128];

	Caption="Previewing " + Filename.UpperCase();

    Terminal->Clear();

    FILE* fp=fopen(Filename.c_str(),"rb");

    if(fp==NULL)
	    return;

    int ch;
	while(!feof(fp)) {
    	ch=fgetc(fp);
        if(ch==EOF)
        	break;
		if(ch==1) { /* ctrl-a */
			ch=fgetc(fp);
			if(ch==EOF)
				break;
			switch(toupper(ch)) {
				case 'A':
					sprintf(str,"\1");
					break;
				case '<':
					sprintf(str,"\b");
					break;
				case '>':
					sprintf(str,ANSI_ESC"K");
					break;
				case '[':
					sprintf(str,"\r");
					break;
				case ']':
					sprintf(str,"\n");
					break;
				case 'L':
					sprintf(str,ANSI_ESC"2J");
					break;
				case '-':
				case '_':
				case 'N':
					sprintf(str,ANSI_ESC"0m");
					break;
				case 'H':
					sprintf(str,ANSI_ESC"1m");
					break;
				case 'I':
					sprintf(str,ANSI_ESC"5m");
					break;
				case 'K':
					sprintf(str,ANSI_ESC"30m");
					break;
				case 'R':
					sprintf(str,ANSI_ESC"31m");
					break;
				case 'G':
					sprintf(str,ANSI_ESC"32m");
					break;
				case 'Y':
					sprintf(str,ANSI_ESC"33m");
					break;
				case 'B':
					sprintf(str,ANSI_ESC"34m");
					break;
				case 'M':
					sprintf(str,ANSI_ESC"35m");
					break;
				case 'C':
					sprintf(str,ANSI_ESC"36m");
					break;
				case 'W':
					sprintf(str,ANSI_ESC"37m");
					break;
				case '0':
					sprintf(str,ANSI_ESC"40m");
					break;
				case '1':
					sprintf(str,ANSI_ESC"41m");
					break;
				case '2':
					sprintf(str,ANSI_ESC"42m");
					break;
				case '3':
					sprintf(str,ANSI_ESC"43m");
					break;
				case '4':
					sprintf(str,ANSI_ESC"44m");
					break;
				case '5':
					sprintf(str,ANSI_ESC"45m");
					break;
				case '6':
					sprintf(str,ANSI_ESC"46m");
					break;
				case '7':
					sprintf(str,ANSI_ESC"47m");
					break;
                case '.':
                case ',':
                case ';':
                	/* ignore delay codes */
                	continue;
				default:
                    if(ch>=0x7f) 	/* move cursor right x columns */
                        sprintf(str,ANSI_ESC"%uC",ch-0x7f);
                    else
						sprintf(str,"\1%c",ch);
					break;
			}
            Terminal->WriteBuffer(str,strlen(str));
		}
		else
			Terminal->WriteBuffer(&ch,1);
    }
    fclose(fp);
}
//---------------------------------------------------------------------------
