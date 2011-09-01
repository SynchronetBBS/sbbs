object LoginAttemptsForm: TLoginAttemptsForm
  Left = 522
  Top = 454
  Width = 496
  Height = 793
  Caption = 'Failed Login Attempts'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnClose = FormClose
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object ListView: TListView
    Left = 0
    Top = 0
    Width = 488
    Height = 766
    Align = alClient
    Columns = <
      item
        Caption = 'Count'
      end
      item
        AutoSize = True
        Caption = 'Address'
      end
      item
        Caption = 'Protocol'
      end
      item
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
    TabOrder = 0
    ViewStyle = vsReport
    OnColumnClick = ListViewColumnClick
    OnCompare = ListViewCompare
  end
end
