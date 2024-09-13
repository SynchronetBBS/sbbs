object ClientForm: TClientForm
  Left = 476
  Top = 436
  Width = 631
  Height = 345
  Caption = 'Clients'
  Color = clBtnFace
  DefaultMonitor = dmPrimary
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWhite
  Font.Height = -12
  Font.Name = 'Microsoft Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnHide = FormHide
  PixelsPerInch = 96
  TextHeight = 15
  object ListView: TListView
    Left = 0
    Top = 0
    Width = 615
    Height = 306
    Align = alClient
    Color = clBlack
    Columns = <
      item
        Caption = 'Socket'
        Width = 57
      end
      item
        Caption = 'Protocol'
        Width = 61
      end
      item
        Caption = 'User'
        Width = 84
      end
      item
        Caption = 'Address'
        Width = 93
      end
      item
        Caption = 'Host Name'
        Width = 93
      end
      item
        Caption = 'Port'
        Width = 52
      end
      item
        AutoSize = True
        Caption = 'Time'
      end>
    ColumnClick = False
    MultiSelect = True
    ReadOnly = True
    RowSelect = True
    PopupMenu = PopupMenu
    TabOrder = 0
    ViewStyle = vsReport
  end
  object Timer: TTimer
    Interval = 5000
    OnTimer = TimerTimer
    Left = 168
    Top = 128
  end
  object PopupMenu: TPopupMenu
    Left = 344
    Top = 200
    object CloseSocketMenuItem: TMenuItem
      Caption = '&Close Socket'
      ShortCut = 46
      OnClick = CloseSocketMenuItemClick
    end
    object FilterIpMenuItem: TMenuItem
      Caption = '&Filter IP Address'
      ShortCut = 16454
      OnClick = FilterIpMenuItemClick
    end
    object N1: TMenuItem
      Caption = '-'
    end
    object SelectAllMenuItem: TMenuItem
      Caption = 'Select All'
      ShortCut = 16449
      OnClick = SelectAllMenuItemClick
    end
  end
end
