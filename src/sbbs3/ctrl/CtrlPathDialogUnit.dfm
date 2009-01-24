object CtrlPathDialog: TCtrlPathDialog
  Left = 352
  Top = 625
  ActiveControl = Edit
  BorderStyle = bsDialog
  Caption = 'Synchronet Configuration File'
  ClientHeight = 114
  ClientWidth = 473
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    473
    114)
  PixelsPerInch = 120
  TextHeight = 16
  object Bevel1: TBevel
    Left = 10
    Top = 10
    Width = 346
    Height = 93
    Anchors = [akLeft, akTop, akBottom]
    Shape = bsFrame
  end
  object OKBtn: TButton
    Left = 369
    Top = 10
    Width = 93
    Height = 31
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
  end
  object CancelBtn: TButton
    Left = 369
    Top = 47
    Width = 93
    Height = 31
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object Edit: TEdit
    Left = 25
    Top = 25
    Width = 320
    Height = 24
    TabOrder = 2
    Text = 'c:\sbbs\ctrl\main.cnf'
  end
  object BrowseButton: TButton
    Left = 25
    Top = 63
    Width = 93
    Height = 31
    Caption = 'Browse...'
    TabOrder = 3
    OnClick = BrowseButtonClick
  end
  object OpenDialog: TOpenDialog
    FileName = 'main.cnf'
    Filter = 'Synchronet Configuration File|main.cnf'
    Left = 424
    Top = 88
  end
end
