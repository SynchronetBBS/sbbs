object PreviewForm: TPreviewForm
  Left = 504
  Top = 375
  Width = 585
  Height = 364
  Caption = 'Preview'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -14
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  PopupMenu = PopupMenu
  Position = poDefaultPosOnly
  OnClose = FormClose
  OnShow = FormShow
  PixelsPerInch = 120
  TextHeight = 16
  object PopupMenu: TPopupMenu
    Left = 144
    Top = 24
    object ChangeFontMenuItem: TMenuItem
      Caption = 'Change Font'
      Hint = 'Change Font'
      OnClick = ChangeFontMenuItemClick
    end
  end
end
