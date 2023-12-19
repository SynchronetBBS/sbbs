object CodeInputForm: TCodeInputForm
  Left = 569
  Top = 374
  BorderStyle = bsDialog
  Caption = 'Parameter Required'
  ClientHeight = 78
  ClientWidth = 356
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    356
    78)
  PixelsPerInch = 96
  TextHeight = 14
  object Bevel1: TBevel
    Left = 9
    Top = 9
    Width = 245
    Height = 58
    Anchors = [akLeft, akTop, akRight, akBottom]
    Shape = bsFrame
  end
  object Label: TLabel
    Left = 20
    Top = 18
    Width = 113
    Height = 22
    Alignment = taRightJustify
    AutoSize = False
    Caption = 'Name/ID/Code'
  end
  object OKBtn: TButton
    Left = 266
    Top = 9
    Width = 81
    Height = 27
    Anchors = [akTop, akRight]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
  end
  object CancelBtn: TButton
    Left = 266
    Top = 41
    Width = 81
    Height = 27
    Anchors = [akTop, akRight]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object ComboBox: TComboBox
    Left = 140
    Top = 16
    Width = 105
    Height = 22
    ItemHeight = 14
    Sorted = True
    TabOrder = 2
  end
  object Edit: TEdit
    Left = 140
    Top = 16
    Width = 105
    Height = 22
    TabOrder = 3
    Visible = False
  end
  object CheckBox: TCheckBox
    Left = 169
    Top = 44
    Width = 74
    Height = 17
    Alignment = taLeftJustify
    BiDiMode = bdLeftToRight
    Caption = 'CheckBox'
    ParentBiDiMode = False
    TabOrder = 4
    Visible = False
  end
end
