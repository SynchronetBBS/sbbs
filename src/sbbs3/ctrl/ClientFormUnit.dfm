object ClientForm: TClientForm
  Left = 476
  Top = 436
  Width = 631
  Height = 345
  Caption = 'Clients'
  Color = clBtnFace
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -13
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnHide = FormHide
  OnShow = FormShow
  PixelsPerInch = 120
  TextHeight = 16
  object ListView: TListView
    Left = 0
    Top = 0
    Width = 623
    Height = 317
    Align = alClient
    Columns = <
      item
        Caption = 'Socket'
        Width = 60
      end
      item
        Caption = 'Protocol'
        Width = 65
      end
      item
        Caption = 'User'
        Width = 90
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
        Caption = 'Port'
        Width = 55
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
      OnClick = CloseSocketMenuItemClick
    end
    object FilterIpMenuItem: TMenuItem
      Caption = '&Filter IP Address'
      OnClick = FilterIpMenuItemClick
    end
  end
end
