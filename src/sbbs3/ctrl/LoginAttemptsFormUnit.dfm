object LoginAttemptsForm: TLoginAttemptsForm
  Left = 729
  Top = 203
  Width = 496
  Height = 793
  BorderIcons = [biSystemMenu, biMaximize]
  Caption = 'Failed Login Attempts'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
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
    Width = 480
    Height = 755
    Align = alClient
    Columns = <
      item
        Caption = 'Unique Attempts'
      end
      item
        Caption = 'Duplicate Attempts'
      end
      item
        AutoSize = True
        Caption = 'Client Address'
      end
      item
        AutoSize = True
        Caption = 'Protocol Last Attempted'
      end
      item
        AutoSize = True
        Caption = 'Username Last Attempted'
      end
      item
        AutoSize = True
        Caption = 'Password Last Attempted'
      end
      item
        AutoSize = True
        Caption = 'Time of Last Attempt'
      end>
    MultiSelect = True
    ReadOnly = True
    RowSelect = True
    PopupMenu = PopupMenu
    TabOrder = 0
    ViewStyle = vsReport
    OnColumnClick = ListViewColumnClick
    OnCompare = ListViewCompare
  end
  object PopupMenu: TPopupMenu
    Left = 168
    Top = 184
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
    object ResolveHostnameMenuItem: TMenuItem
      Caption = 'Lookup Hostname'
      OnClick = ResolveHostnameMenuItemClick
    end
    object FilterIpMenuItem: TMenuItem
      Caption = 'Filter IP Address'
      OnClick = FilterIpMenuItemClick
    end
    object ClearListMenuItem: TMenuItem
      Caption = 'Clear List'
      OnClick = ClearListMenuItemClick
    end
  end
end
