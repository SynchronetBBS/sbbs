object TelnetCfgDlg: TTelnetCfgDlg
  Left = 484
  Top = 622
  BorderStyle = bsDialog
  Caption = 'Telnet Server Configuration'
  ClientHeight = 234
  ClientWidth = 286
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object PageControl: TPageControl
    Left = 3
    Top = 3
    Width = 278
    Height = 186
    ActivePage = GeneralTabSheet
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object InterfaceLabel: TLabel
        Left = 7
        Top = 29
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Interface (IP)'
      end
      object TelnetPortLabel: TLabel
        Left = 7
        Top = 55
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Port'
      end
      object FirstNodeLabel: TLabel
        Left = 7
        Top = 81
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'First Node'
      end
      object LastNodeLabel: TLabel
        Left = 7
        Top = 107
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Last Node'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 7
        Top = 5
        Width = 117
        Height = 19
        Hint = 
          'Check this box if you want the Telnet server to automatically st' +
          'art'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object NetworkInterfaceEdit: TEdit
        Left = 85
        Top = 29
        Width = 156
        Height = 21
        Hint = 
          'Enter your Network adapter'#39's static IP address here or blank for' +
          ' <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object TelnetPortEdit: TEdit
        Left = 85
        Top = 55
        Width = 39
        Height = 21
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object KeepAliveCheckBox: TCheckBox
        Left = 148
        Top = 55
        Width = 117
        Height = 20
        Hint = 
          'Check this box if you want to configure WinSock to keep your dia' +
          'l-up connection active'
        Caption = 'Send Keep Alives'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object FirstNodeEdit: TEdit
        Left = 85
        Top = 81
        Width = 39
        Height = 21
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object XtrnMinCheckBox: TCheckBox
        Left = 148
        Top = 80
        Width = 117
        Height = 20
        Hint = 
          'Check this box if you want external programs to run in a minimiz' +
          'ed window'
        Caption = 'Minimize externals'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object LastNodeEdit: TEdit
        Left = 85
        Top = 107
        Width = 39
        Height = 21
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object AutoLogonCheckBox: TCheckBox
        Left = 148
        Top = 106
        Width = 117
        Height = 19
        Hint = 
          'Check this box if you want your V exempt users to be able to aut' +
          'o-logon based on their IP address'
        Caption = 'AutoLogon via IP'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object HostnameCheckBox: TCheckBox
        Left = 148
        Top = 5
        Width = 117
        Height = 19
        Hint = 
          'Check this box if you want to automatically lookup client'#39's host' +
          'names via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
    end
    object LogTabSheet: TTabSheet
      Caption = 'Log'
      ImageIndex = 1
      object CmdLogCheckBox: TCheckBox
        Left = 7
        Top = 5
        Width = 169
        Height = 19
        Hint = 
          'Check this box if you want to log (debug) all received Telnet co' +
          'mmands'
        Caption = 'Received Commands'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 2
      object AnswerSoundLabel: TLabel
        Left = 7
        Top = 11
        Width = 52
        Height = 20
        AutoSize = False
        Caption = 'Answer'
      end
      object HnagupSoundLabel: TLabel
        Left = 7
        Top = 37
        Width = 52
        Height = 20
        AutoSize = False
        Caption = 'Hangup'
      end
      object AnswerSoundEdit: TEdit
        Left = 72
        Top = 11
        Width = 169
        Height = 21
        TabOrder = 0
      end
      object AnswerSoundButton: TButton
        Left = 247
        Top = 11
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 1
        OnClick = AnswerSoundButtonClick
      end
      object HangupSoundEdit: TEdit
        Left = 72
        Top = 37
        Width = 169
        Height = 21
        TabOrder = 2
      end
      object HangupSoundButton: TButton
        Left = 247
        Top = 37
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 3
        OnClick = HangupSoundButtonClick
      end
    end
  end
  object OKBtn: TButton
    Left = 20
    Top = 201
    Width = 76
    Height = 25
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 104
    Top = 201
    Width = 75
    Height = 25
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object ApplyBtn: TButton
    Left = 189
    Top = 201
    Width = 76
    Height = 25
    Cancel = True
    Caption = 'Apply'
    TabOrder = 3
    OnClick = OKBtnClick
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Top = 248
  end
end
