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
    TMenuItem *CopyPopup;
    TMenuItem *CopyAllPopup;
    TMenuItem *RefreshPopup;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall ListViewColumnClick(TObject *Sender,
          TListColumn *Column);
    void __fastcall ListViewCompare(TObject *Sender, TListItem *Item1,
          TListItem *Item2, int Data, int &Compare);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall EditUserPopupClick(TObject *Sender);
    void __fastcall ListViewKeyPress(TObject *Sender, char &Key);
    void __fastcall CopyPopupClick(TObject *Sender);
    void __fastcall CopyAllPopupClick(TObject *Sender);
    void __fastcall RefreshPopupClick(TObject *Sender);
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
