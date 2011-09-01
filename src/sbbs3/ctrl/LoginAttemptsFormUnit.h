//---------------------------------------------------------------------------

#ifndef LoginAttemptsFormUnitH
#define LoginAttemptsFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
//---------------------------------------------------------------------------
class TLoginAttemptsForm : public TForm
{
__published:	// IDE-managed Components
    TListView *ListView;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall ListViewColumnClick(TObject *Sender,
          TListColumn *Column);
    void __fastcall ListViewCompare(TObject *Sender, TListItem *Item1,
          TListItem *Item2, int Data, int &Compare);
private:	// User declarations
public:		// User declarations
    __fastcall TLoginAttemptsForm(TComponent* Owner);
    int ColumnToSort;
    bool SortBackwards;
};
//---------------------------------------------------------------------------
extern PACKAGE TLoginAttemptsForm *LoginAttemptsForm;
//---------------------------------------------------------------------------
#endif
