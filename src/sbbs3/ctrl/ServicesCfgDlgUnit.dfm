object ServicesCfgDlg: TServicesCfgDlg
  Left = 578
  Top = 317
  BorderStyle = bsDialog
  Caption = 'Services Configuration'
  ClientHeight = 301
  ClientWidth = 352
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -13
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  Icon.Data = {
    0000010001002020100000000000E80200001600000028000000200000004000
    0000010004000000000080020000000000000000000000000000000000000000
    000000008000008000000080800080000000800080008080000080808000C0C0
    C0000000FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF000000
    0000000000000000000000000000000000000000007777770000000000000000
    0000000000000007000000000000000000000000000000070000000000000000
    0000000000777007000000000000000000077070007770070000700000000000
    0077000700787000000007000000000007708000077877000070007000000000
    07088807777777770777000700000000008F88877FFFFF077887700700000000
    00088888F88888FF08870070000000000000880888877778F070007000000007
    77088888880007778F770077777000700008F088007777077F07000000700700
    008F08880800077778F7700000700708888F0880F08F807078F7777700700708
    F88F0780F070F07078F7887700700708888F0780F077807088F7777700700700
    008F0788FF00080888F77000007000000008F0780FFFF0088F77007000000000
    0008F07788000888887700700000000000008F07788888880870007000000000
    00088FF0077788088887000700000000008F888FF00000F87887700700000000
    0708F8088FFFFF88078700700000000007708000088888000070070000000000
    0077007000888007000070000000000000077700008F80070007000000000000
    0000000000888007000000000000000000000000000000070000000000000000
    000000000777777700000000000000000000000000000000000000000000FFFF
    FFFFFFFC0FFFFFFC0FFFFFF80FFFFFF80FFFFE180E7FFC00043FF800001FF800
    000FF800000FFC00001FFE00001FE0000001C000000180000001800000018000
    00018000000180000001FC00001FFC00001FFE00001FFC00000FF800000FF800
    001FF800003FFC180C7FFE380EFFFFF80FFFFFF80FFFFFF80FFFFFFFFFFF}
  OldCreateOrder = False
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    352
    301)
  PixelsPerInch = 120
  TextHeight = 16
  object PageControl: TPageControl
    Left = 4
    Top = 4
    Width = 342
    Height = 245
    ActivePage = SoundTabSheet
    TabIndex = 1
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object InterfaceLabel: TLabel
        Left = 9
        Top = 42
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'Interface (IP)'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 144
        Height = 25
        Hint = 'Automatically start Services'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object NetworkInterfaceEdit: TEdit
        Left = 105
        Top = 42
        Width = 192
        Height = 24
        Hint = 'Your network adapter'#39's static IP address or blank for <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object HostnameCheckBox: TCheckBox
        Left = 182
        Top = 12
        Width = 147
        Height = 25
        Hint = 'Automatically lookup client'#39's hostnames via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object ServicesCfgButton: TButton
        Left = 8
        Top = 80
        Width = 97
        Height = 25
        Hint = 'Edut services configuration file'
        Caption = 'services.cfg'
        TabOrder = 3
        OnClick = ServicesCfgButtonClick
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 2
      object AnswerSoundLabel: TLabel
        Left = 9
        Top = 12
        Width = 80
        Height = 25
        AutoSize = False
        Caption = 'Connect'
      end
      object HangupSoundLabel: TLabel
        Left = 9
        Top = 44
        Width = 80
        Height = 25
        AutoSize = False
        Caption = 'Disconnect'
      end
      object AnswerSoundEdit: TEdit
        Left = 105
        Top = 12
        Width = 192
        Height = 24
        Hint = 'Sound file to play when users connect'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object AnswerSoundButton: TButton
        Left = 304
        Top = 12
        Width = 25
        Height = 26
        Caption = '...'
        TabOrder = 1
        OnClick = AnswerSoundButtonClick
      end
      object HangupSoundEdit: TEdit
        Left = 105
        Top = 44
        Width = 192
        Height = 24
        Hint = 'Sound file to play when users disconnect'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object HangupSoundButton: TButton
        Left = 304
        Top = 44
        Width = 25
        Height = 26
        Caption = '...'
        TabOrder = 3
        OnClick = HangupSoundButtonClick
      end
    end
  end
  object OKBtn: TButton
    Left = 25
    Top = -47
    Width = 93
    Height = 30
    Anchors = [akLeft, akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
  end
  object CancelBtn: TButton
    Left = 128
    Top = -47
    Width = 92
    Height = 30
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object ApplyBtn: TButton
    Left = 233
    Top = -47
    Width = 93
    Height = 30
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Apply'
    TabOrder = 3
  end
  object OKButton: TButton
    Left = 25
    Top = 260
    Width = 93
    Height = 30
    Anchors = [akLeft, akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 4
    OnClick = OKButtonClick
  end
  object CancelButton: TButton
    Left = 128
    Top = 260
    Width = 92
    Height = 30
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 5
  end
  object ApplyButton: TButton
    Left = 233
    Top = 260
    Width = 93
    Height = 30
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Apply'
    TabOrder = 6
    OnClick = OKButtonClick
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Top = 240
  end
end
