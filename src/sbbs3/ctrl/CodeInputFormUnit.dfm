object CodeInputForm: TCodeInputForm
  Left = 569
  Top = 374
  BorderStyle = bsDialog
  Caption = 'Parameter Required'
  ClientHeight = 73
  ClientWidth = 331
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    331
    73)
  PixelsPerInch = 96
  TextHeight = 13
  object Bevel1: TBevel
    Left = 8
    Top = 8
    Width = 228
    Height = 55
    Anchors = [akLeft, akTop, akRight, akBottom]
    Shape = bsFrame
  end
  object Label: TLabel
    Left = 13
    Top = 26
    Width = 105
    Height = 20
    Alignment = taRightJustify
    AutoSize = False
    Caption = 'Name/ID/Code'
  end
  object OKBtn: TButton
    Left = 247
    Top = 8
    Width = 75
    Height = 25
    Anchors = [akTop, akRight]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
  end
  object CancelBtn: TButton
    Left = 247
    Top = 38
    Width = 75
    Height = 25
    Anchors = [akTop, akRight]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object ComboBox: TComboBox
    Left = 124
    Top = 26
    Width = 98
    Height = 21
    ItemHeight = 13
    Sorted = True
    TabOrder = 2
  end
  object Edit: TEdit
    Left = 124
    Top = 26
    Width = 98
    Height = 21
    TabOrder = 3
    Visible = False
  end
end
