object UserListForm: TUserListForm
  Left = 380
  Top = 245
  Width = 870
  Height = 640
  BorderIcons = [biSystemMenu, biMaximize]
  Caption = 'Synchronet User List'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -10
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  FormStyle = fsStayOnTop
  OldCreateOrder = False
  Position = poDefault
  OnClose = FormClose
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object ListView: TListView
    Left = 0
    Top = 0
    Width = 854
    Height = 601
    Align = alClient
    Columns = <
      item
        Caption = 'Num'
        Width = 33
      end
      item
        Caption = 'Alias'
        Width = 81
      end
      item
        Caption = 'Name'
        Width = 81
      end
      item
        Caption = 'Lev'
        Width = 33
      end
      item
        Caption = 'Age'
        Width = 33
      end
      item
        Caption = 'Sex'
        Width = 33
      end
      item
        Caption = 'Location'
        Width = 81
      end
      item
        Caption = 'Protocol'
        Width = 57
      end
      item
        Caption = 'Address'
        Width = 81
      end
      item
        Caption = 'Host Name'
        Width = 81
      end
      item
        Caption = 'Phone'
        Width = 81
      end
      item
        Caption = 'Email'
        Width = 81
      end
      item
        Caption = 'Logons'
        Width = 49
      end
      item
        Caption = 'First On'
        Width = 57
      end
      item
        Caption = 'Last On'
        Width = 57
      end>
    ReadOnly = True
    RowSelect = True
    PopupMenu = PopupMenu
    TabOrder = 0
    ViewStyle = vsReport
    OnColumnClick = ListViewColumnClick
    OnCompare = ListViewCompare
    OnDblClick = EditUserPopupClick
    OnKeyPress = ListViewKeyPress
  end
  object PopupMenu: TPopupMenu
    Left = 232
    Top = 96
    object EditUserPopup: TMenuItem
      Caption = 'Edit User'
      OnClick = EditUserPopupClick
    end
    object CopyPopup: TMenuItem
      Caption = 'Copy'
      ShortCut = 16451
      OnClick = CopyPopupClick
    end
    object CopyAllPopup: TMenuItem
      Caption = 'Copy All'
      ShortCut = 16449
      OnClick = CopyAllPopupClick
    end
    object RefreshPopup: TMenuItem
      Caption = 'Refresh'
      ShortCut = 116
      OnClick = RefreshPopupClick
    end
  end
end
