//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "UserMsgFormUnit.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TUserMsgForm *UserMsgForm;
//---------------------------------------------------------------------------
__fastcall TUserMsgForm::TUserMsgForm(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
