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
#include <Dialogs.hpp>
//----------------------------------------------------------------------------
class TPropertiesDlg : public TForm
{
__published:
	TButton *OKBtn;
	TButton *CancelBtn;
	TPageControl *PageControl;
	TTabSheet *SettingsTabSheet;
	TLabel *Label3;
	TEdit *LoginCmdEdit;
	TLabel *Label2;
	TEdit *ConfigCmdEdit;
	TLabel *Label4;
	TEdit *NodeIntEdit;
	TUpDown *NodeIntUpDown;
	TLabel *Label5;
	TEdit *ClientIntEdit;
	TUpDown *ClientIntUpDown;
	TCheckBox *UndockableCheckBox;
	TCheckBox *TrayIconCheckBox;
	TLabel *PasswordLabel;
	TEdit *PasswordEdit;
	TTabSheet *CustomizeTabSheet;
	TComboBox *SourceComboBox;
	TEdit *ExampleEdit;
	TButton *FontButton;
	TButton *BackgroundButton;
	TFontDialog *FontDialog1;
	TButton *ApplyButton;
	TComboBox *TargetComboBox;
	TColorDialog *ColorDialog1;
        TTabSheet *AdvancedTabSheet;
        TLabel *Label1;
        TEdit *CtrlDirEdit;
        TLabel *Label6;
        TEdit *HostnameEdit;
	TLabel *Label7;
	TEdit *JS_MaxBytesEdit;
	TLabel *Label8;
	TEdit *MaxLogLenEdit;
	TLabel *Label9;
	TEdit *TempDirEdit;
	void __fastcall FormShow(TObject *Sender);
	void __fastcall TrayIconCheckBoxClick(TObject *Sender);
	void __fastcall SourceComboBoxChange(TObject *Sender);
	void __fastcall FontButtonClick(TObject *Sender);
	void __fastcall BackgroundButtonClick(TObject *Sender);
	void __fastcall ApplyButtonClick(TObject *Sender);
private:
public:
	virtual __fastcall TPropertiesDlg(TComponent* AOwner);
	void __fastcall TPropertiesDlg::ChangeScheme(int target);
};
//----------------------------------------------------------------------------
extern PACKAGE TPropertiesDlg *PropertiesDlg;
//----------------------------------------------------------------------------
#endif    
