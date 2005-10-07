//---------------------------------------------------------------------------

#ifndef MainUnitH
#define MainUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
#include <jpeg.hpp>
#include "sbbs.h"
//---------------------------------------------------------------------------
class TConfigWizard : public TForm
{
__published:	// IDE-managed Components
    TShape *Shape1;
    TButton *BackButton;
    TButton *NextButton;
    TButton *CancelButton;
    TNotebook *WizNotebook;
    TMemo *IntroMemo;
    TLabel *TitleLabel;
    TLabel *IntroductionLabel;
    TLabel *IdentificationLabel;
    TLabel *QWKLabel;
    TLabel *InternetLabel;
    TLabel *MaximumsLabel;
    TLabel *OptionsLabel;
    TLabel *Label1;
    TEdit *SysopNameEdit;
    TEdit *SystemNameEdit;
    TLabel *SystemNameLabel;
    TLabel *Label2;
    TEdit *SystemLocationEdit;
    TLabel *Label3;
    TEdit *SystemPasswordEdit;
    TLabel *Label4;
    TLabel *Label6;
    TEdit *NodesEdit;
    TUpDown *NodesUpDown;
    TLabel *Label7;
    TEdit *MaxFtpEdit;
    TUpDown *MaxFtpUpDown;
    TLabel *Label8;
    TEdit *MaxMailEdit;
    TUpDown *MaxMailUpDown;
    TLabel *Label9;
    TEdit *QWKIDEdit;
    TLabel *Label10;
    TEdit *QNetTaglineEdit;
    TProgressBar *ProgressBar;
    TLabel *VerifyLabel;
    TImage *LogoImage;
    TMemo *CompleteMemo;
    TEdit *SysPassVerifyEdit;
    TLabel *CompleteLabel;
    TMemo *SysPassVerificationMemo;
    TLabel *Label11;
    TMemo *Memo1;
    TLabel *IllegalCharsLabel;
    TLabel *TimeLabel;
    TGroupBox *GroupBox1;
    TRadioButton *Time12hrRadioButton;
    TRadioButton *Time24hrRadioButton;
    TGroupBox *GroupBox2;
    TRadioButton *DateUsRadioButton;
    TRadioButton *DateEuRadioButton;
    TGroupBox *GroupBox3;
    TComboBox *TimeZoneComboBox;
    TCheckBox *DaylightCheckBox;
    TComboBox *InternetAddressComboBox;
    TCheckBox *AllNodesTelnetCheckBox;
    TLabel *Label12;
    TLabel *Label13;
    TGroupBox *GroupBox4;
    TGroupBox *GroupBox5;
    TRadioButton *DeletedEmailYesButton;
    TRadioButton *DeletedEmailNoButton;
    TCheckBox *FeedbackCheckBox;
    TCheckBox *NewUsersCheckBox;
    TRadioButton *DeletedEmailSysopButton;
    TCheckBox *AliasesCheckBox;
    TLabel *Label15;
    TEdit *MaxWebEdit;
    TUpDown *MaxWebUpDown;
    void __fastcall NextButtonClick(TObject *Sender);
    void __fastcall CancelButtonClick(TObject *Sender);
    void __fastcall BackButtonClick(TObject *Sender);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall LogoImageClick(TObject *Sender);
    void __fastcall TitleLabelClick(TObject *Sender);
    void __fastcall WizNotebookPageChanged(TObject *Sender);
    void __fastcall VerifyIdentification(TObject *Sender);
    void __fastcall VerifyPassword(TObject *Sender);
    void __fastcall VerifyQWK(TObject *Sender);
    void __fastcall QNetTaglineEditKeyPress(TObject *Sender, char &Key);
    void __fastcall VerifyInternetAddresses(TObject *Sender);
    void __fastcall TimeZoneComboBoxChange(TObject *Sender);
    void __fastcall NewUsersCheckBoxClick(TObject *Sender);
private:	// User declarations
    scfg_t  scfg;
public:		// User declarations
    __fastcall TConfigWizard(TComponent* Owner);
};
//---------------------------------------------------------------------------
#endif
