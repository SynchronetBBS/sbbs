object UserListForm: TUserListForm
  Left = 380
  Top = 245
  Width = 870
  Height = 640
  Caption = 'Synchronet User List'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -13
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  FormStyle = fsStayOnTop
  OldCreateOrder = False
  Position = poDefault
  OnClose = FormClose
  OnShow = FormShow
  PixelsPerInch = 120
  TextHeight = 16
  object ListView: TListView
    Left = 0
    Top = 0
    Width = 862
    Height = 607
    Align = alClient
    Columns = <
      item
        Caption = 'Num'
        Width = 40
      end
      item
        Caption = 'Alias'
        Width = 100
      end
      item
        Caption = 'Name'
        Width = 100
      end
      item
        Caption = 'Lev'
        Width = 40
      end
      item
        Caption = 'Age'
        Width = 40
      end
      item
        Caption = 'Sex'
        Width = 40
      end
      item
        Caption = 'Location'
        Width = 100
      end
      item
        Caption = 'Protocol'
        Width = 70
      end
      item
        Caption = 'Address'
        Width = 100
      end
      item
        Caption = 'Host Name'
        Width = 100
      end
      item
        Caption = 'Phone'
        Width = 100
      end
      item
        Caption = 'Email'
        Width = 100
      end
      item
        Caption = 'Logons'
        Width = 60
      end
      item
        Caption = 'First On'
        Width = 70
      end
      item
        Caption = 'Last On'
        Width = 70
      end>
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
  end
end
