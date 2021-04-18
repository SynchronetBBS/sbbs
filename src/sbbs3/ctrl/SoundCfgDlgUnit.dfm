object SoundCfgDlg: TSoundCfgDlg
  Left = 910
  Top = 472
  BorderStyle = bsDialog
  Caption = 'Sound Configuration'
  ClientHeight = 186
  ClientWidth = 313
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object AnswerSoundLabel: TLabel
    Left = 9
    Top = 18
    Width = 65
    Height = 20
    AutoSize = False
    Caption = 'Connect'
  end
  object HnagupSoundLabel: TLabel
    Left = 9
    Top = 44
    Width = 65
    Height = 20
    AutoSize = False
    Caption = 'Disconnect'
  end
  object LoginSoundLabel: TLabel
    Left = 9
    Top = 70
    Width = 65
    Height = 20
    AutoSize = False
    Caption = 'Login'
  end
  object LogoutSoundLabel: TLabel
    Left = 9
    Top = 96
    Width = 65
    Height = 20
    AutoSize = False
    Caption = 'Logout'
  end
  object HackAttemptSoundLabel: TLabel
    Left = 9
    Top = 122
    Width = 65
    Height = 20
    AutoSize = False
    Caption = 'Hack Attempt'
  end
  object OKBtn: TButton
    Left = 79
    Top = 156
    Width = 75
    Height = 25
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 10
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 159
    Top = 156
    Width = 75
    Height = 25
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 11
  end
  object AnswerSoundEdit: TEdit
    Left = 80
    Top = 18
    Width = 201
    Height = 21
    Hint = 'Sound file to play when accepting an incoming connection'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
  end
  object HangupSoundEdit: TEdit
    Left = 80
    Top = 44
    Width = 201
    Height = 21
    Hint = 'Sound file to play when disconnecting'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 2
  end
  object AnswerSoundButton: TButton
    Left = 287
    Top = 18
    Width = 20
    Height = 21
    Caption = '...'
    TabOrder = 1
    OnClick = AnswerSoundButtonClick
  end
  object HangupSoundButton: TButton
    Left = 287
    Top = 44
    Width = 20
    Height = 21
    Caption = '...'
    TabOrder = 3
    OnClick = HangupSoundButtonClick
  end
  object LoginSoundButton: TButton
    Left = 287
    Top = 70
    Width = 20
    Height = 21
    Caption = '...'
    TabOrder = 5
    OnClick = LoginSoundButtonClick
  end
  object LoginSoundEdit: TEdit
    Left = 80
    Top = 70
    Width = 201
    Height = 21
    Hint = 'Sound file to play when accepting an incoming connection'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 4
  end
  object LogoutSoundEdit: TEdit
    Left = 80
    Top = 96
    Width = 201
    Height = 21
    Hint = 'Sound file to play when disconnecting'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 6
  end
  object LogoutSoundButton: TButton
    Left = 287
    Top = 96
    Width = 20
    Height = 21
    Caption = '...'
    TabOrder = 7
    OnClick = LogoutSoundButtonClick
  end
  object HackAttemptSoundEdit: TEdit
    Left = 80
    Top = 122
    Width = 201
    Height = 21
    Hint = 'Sound file to play when disconnecting'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 8
  end
  object HackAttemptSoundButton: TButton
    Left = 287
    Top = 122
    Width = 20
    Height = 21
    Caption = '...'
    TabOrder = 9
    OnClick = HackAttemptSoundButtonClick
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Options = [ofNoChangeDir, ofEnableSizing]
    Left = 8
    Top = 144
  end
end
