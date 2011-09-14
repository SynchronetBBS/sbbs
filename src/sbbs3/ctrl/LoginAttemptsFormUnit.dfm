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
        Caption = 'Unique'
      end
      item
        Caption = 'Dupes'
      end
      item
        AutoSize = True
        Caption = 'Address'
      end
      item
        AutoSize = True
        Caption = 'Protocol'
      end
      item
        AutoSize = True
        Caption = 'User'
      end
      item
        AutoSize = True
        Caption = 'Password'
      end
      item
        AutoSize = True
        Caption = 'Time'
      end>
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
  end
end
