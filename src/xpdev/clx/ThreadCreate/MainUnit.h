//---------------------------------------------------------------------------

#ifndef MainUnitH
#define MainUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <SyncObjs.hpp>
#include <QControls.hpp>
#include <QStdCtrls.hpp>
#include <QForms.hpp>
#include <QComCtrls.hpp>
#include <QExtCtrls.hpp>
#include <QTypes.hpp>
//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
	TListBox *ListBox;
	TButton *CreateButton;
	TButton *KillButton;
	TMemo *Log;
	TStatusBar *StatusBar;
	TTimer *Timer;
	void __fastcall CreateButtonClick(TObject *Sender);
	void __fastcall KillButtonClick(TObject *Sender);
	void __fastcall TimerTick(TObject *Sender);
private:	// User declarations
public:		// User declarations
	TList* List;
	TCriticalSection* CriticalSection;
	__fastcall TForm1(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
