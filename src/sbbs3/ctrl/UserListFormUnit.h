//---------------------------------------------------------------------------

#ifndef UserListFormUnitH
#define UserListFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TUserListForm : public TForm
{
__published:	// IDE-managed Components
    TListView *ListView;
    TPopupMenu *PopupMenu;
    TMenuItem *EditUserPopup;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall ListViewColumnClick(TObject *Sender,
          TListColumn *Column);
    void __fastcall ListViewCompare(TObject *Sender, TListItem *Item1,
          TListItem *Item2, int Data, int &Compare);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall EditUserPopupClick(TObject *Sender);
    void __fastcall ListViewKeyPress(TObject *Sender, char &Key);
private:	// User declarations
public:		// User declarations
    __fastcall TUserListForm(TComponent* Owner);
    int ColumnToSort;
    bool SortBackwards;
};
//---------------------------------------------------------------------------
extern PACKAGE TUserListForm *UserListForm;
//---------------------------------------------------------------------------
#endif
