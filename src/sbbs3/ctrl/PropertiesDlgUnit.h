//----------------------------------------------------------------------------
#ifndef PropertiesDlgUnitH
#define PropertiesDlgUnitH
//----------------------------------------------------------------------------
#include <vcl\ExtCtrls.hpp>
#include <vcl\Buttons.hpp>
#include <vcl\StdCtrls.hpp>
#include <vcl\Controls.hpp>
#include <vcl\Forms.hpp>
#include <vcl\Graphics.hpp>
#include <vcl\Classes.hpp>
#include <vcl\SysUtils.hpp>
#include <vcl\Windows.hpp>
#include <vcl\System.hpp>
#include <ComCtrls.hpp>
//----------------------------------------------------------------------------
class TPropertiesDlg : public TForm
{
__published:
	TButton *OKBtn;
	TButton *CancelBtn;
	TBevel *Bevel1;
    TLabel *Label1;
    TEdit *CtrlDirEdit;
    TLabel *Label2;
    TEdit *ConfigCmdEdit;
    TLabel *Label3;
    TEdit *LoginCmdEdit;
    TCheckBox *TrayIconCheckBox;
    TLabel *Label4;
    TEdit *NodeIntEdit;
    TUpDown *NodeIntUpDown;
    TLabel *Label5;
    TEdit *ClientIntEdit;
    TUpDown *ClientIntUpDown;
    TCheckBox *UndockableCheckBox;
	TLabel *PasswordLabel;
	TEdit *PasswordEdit;
	void __fastcall FormShow(TObject *Sender);
	void __fastcall TrayIconCheckBoxClick(TObject *Sender);
private:
public:
	virtual __fastcall TPropertiesDlg(TComponent* AOwner);
};
//----------------------------------------------------------------------------
extern PACKAGE TPropertiesDlg *PropertiesDlg;
//----------------------------------------------------------------------------
#endif    
