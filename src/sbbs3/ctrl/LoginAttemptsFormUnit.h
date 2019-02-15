//---------------------------------------------------------------------------

#ifndef LoginAttemptsFormUnitH
#define LoginAttemptsFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TLoginAttemptsForm : public TForm
{
__published:	// IDE-managed Components
    TListView *ListView;
    TPopupMenu *PopupMenu;
    TMenuItem *CopyPopup;
    TMenuItem *CopyAllPopup;
    TMenuItem *RefreshPopup;
    TMenuItem *FilterIpMenuItem;
    TMenuItem *ResolveHostnameMenuItem;
    TMenuItem *ClearListMenuItem;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall ListViewColumnClick(TObject *Sender,
          TListColumn *Column);
    void __fastcall ListViewCompare(TObject *Sender, TListItem *Item1,
          TListItem *Item2, int Data, int &Compare);
    void __fastcall CopyPopupClick(TObject *Sender);
    void __fastcall CopyAllPopupClick(TObject *Sender);
    void __fastcall RefreshPopupClick(TObject *Sender);
    void __fastcall FilterIpMenuItemClick(TObject *Sender);
    void __fastcall ResolveHostnameMenuItemClick(TObject *Sender);
    void __fastcall ClearListMenuItemClick(TObject *Sender);
private:	// User declarations
    void __fastcall FillListView(TObject *Sender);
public:		// User declarations
    __fastcall TLoginAttemptsForm(TComponent* Owner);
    int ColumnToSort;
    bool SortBackwards;
};
//---------------------------------------------------------------------------
extern PACKAGE TLoginAttemptsForm *LoginAttemptsForm;
//---------------------------------------------------------------------------
#endif
