object TelnetCfgDlg: TTelnetCfgDlg
  Left = 366
  Top = 194
  BorderStyle = bsDialog
  Caption = 'Telnet Server Configuration'
  ClientHeight = 288
  ClientWidth = 352
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  PixelsPerInch = 120
  TextHeight = 16
  object PageControl: TPageControl
    Left = 4
    Top = 4
    Width = 342
    Height = 229
    ActivePage = TelnetTabSheet
    TabIndex = 1
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object FirstNodeLabel: TLabel
        Left = 9
        Top = 12
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'First Node'
      end
      object LastNodeLabel: TLabel
        Left = 9
        Top = 44
        Width = 91
        Height = 25
        AutoSize = False
        Caption = 'Last Node'
      end
      object XtrnPollsLabel: TLabel
        Left = 9
        Top = 76
        Width = 91
        Height = 25
        AutoSize = False
        Caption = 'External Yield'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 182
        Top = 12
        Width = 144
        Height = 24
        Hint = 'Automatically start Telnet server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object FirstNodeEdit: TEdit
        Left = 105
        Top = 12
        Width = 48
        Height = 24
        Hint = 'First node number available for Telnet logins'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object XtrnMinCheckBox: TCheckBox
        Left = 182
        Top = 44
        Width = 144
        Height = 25
        Hint = 'External programs run in a minimized window'
        Caption = 'Minimize Externals'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object LastNodeEdit: TEdit
        Left = 105
        Top = 44
        Width = 48
        Height = 24
        Hint = 'Last node number available for Telnet logins'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object HostnameCheckBox: TCheckBox
        Left = 9
        Top = 108
        Width = 144
        Height = 24
        Hint = 'Automatically lookup client'#39's hostname via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object QWKEventsCheckBox: TCheckBox
        Left = 182
        Top = 76
        Width = 144
        Height = 25
        Hint = 'Handle QWK Message Packet Events in This Instance'
        Caption = 'QWK Msg Events'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object JavaScriptCheckBox: TCheckBox
        Left = 182
        Top = 108
        Width = 144
        Height = 25
        Hint = 'Enable JavaScript Support'
        Caption = 'JavaScript Support'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object XtrnYieldEdit: TEdit
        Left = 105
        Top = 76
        Width = 48
        Height = 24
        Hint = 
          'Number of polls before yielding time-slices for external DOS/FOS' +
          'SIL programs'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object IdentityCheckBox: TCheckBox
        Left = 9
        Top = 140
        Width = 144
        Height = 24
        Hint = 'Automatically lookup client'#39's identity via IDENT protocol'
        Caption = 'Identity Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
    end
    object TelnetTabSheet: TTabSheet
      Caption = 'Telnet'
      ImageIndex = 1
      object InterfaceLabel: TLabel
        Left = 9
        Top = 44
        Width = 96
        Height = 26
        AutoSize = False
        Caption = 'Interface (IP)'
      end
      object TelnetPortLabel: TLabel
        Left = 9
        Top = 12
        Width = 96
        Height = 26
        AutoSize = False
        Caption = 'Listening Port'
      end
      object CmdLogCheckBox: TCheckBox
        Left = 9
        Top = 108
        Width = 208
        Height = 26
        Hint = 'Log (debug) all transmitted and received Telnet commands'
        Caption = 'Log Telnet Commands'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object TelnetInterfaceEdit: TEdit
        Left = 105
        Top = 44
        Width = 192
        Height = 24
        Hint = 
          'Enter your Network adapter'#39's static IP address here or blank for' +
          ' <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object TelnetPortEdit: TEdit
        Left = 105
        Top = 12
        Width = 48
        Height = 24
        Hint = 'TCP port for incoming connections (default=23)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object TelnetNopCheckBox: TCheckBox
        Left = 9
        Top = 76
        Width = 208
        Height = 26
        Hint = 
          'Send periodic Telnet NOP commands to help detect dropped connect' +
          'ions'
        Caption = 'Send Telnet NOPs'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object AutoLogonCheckBox: TCheckBox
        Left = 9
        Top = 140
        Width = 144
        Height = 24
        Hint = 'Allow V-exempt users to auto-logon based on their IP address'
        Caption = 'AutoLogon via IP'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
    end
    object RLoginTabSheet: TTabSheet
      Caption = 'RLogin'
      ImageIndex = 3
      object RLoginPortLabel: TLabel
        Left = 9
        Top = 12
        Width = 96
        Height = 26
        AutoSize = False
        Caption = 'Listening Port'
      end
      object RLoginInterfaceLabel: TLabel
        Left = 9
        Top = 44
        Width = 96
        Height = 26
        AutoSize = False
        Caption = 'Interface (IP)'
      end
      object RLoginPortEdit: TEdit
        Left = 105
        Top = 12
        Width = 48
        Height = 24
        Hint = 'TCP port for incoming connections (default=513)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object RLoginInterfaceEdit: TEdit
        Left = 105
        Top = 44
        Width = 192
        Height = 24
        Hint = 
          'Enter your Network adapter'#39's static IP address here or blank for' +
          ' <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object RLoginEnabledCheckBox: TCheckBox
        Left = 226
        Top = 12
        Width = 90
        Height = 21
        Hint = 'Enable the RLogin port'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnClick = RLoginEnabledCheckBoxClick
      end
      object RLoginIPallowButton: TButton
        Left = 194
        Top = 76
        Width = 103
        Height = 26
        Hint = 'IP addresses of trusted hosts to allow RLogins from'
        Caption = 'Allowed IPs'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
        OnClick = RLoginIPallowButtonClick
      end
      object RLogin2ndNameCheckBox: TCheckBox
        Left = 9
        Top = 76
        Width = 169
        Height = 26
        Hint = 'Use 2nd RLogin account name for the BBS user name'
        Caption = 'Use 2nd Login Name'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 2
      object AnswerSoundLabel: TLabel
        Left = 9
        Top = 12
        Width = 64
        Height = 25
        AutoSize = False
        Caption = 'Answer'
      end
      object HnagupSoundLabel: TLabel
        Left = 9
        Top = 44
        Width = 64
        Height = 25
        AutoSize = False
        Caption = 'Hangup'
      end
      object AnswerSoundEdit: TEdit
        Left = 89
        Top = 12
        Width = 208
        Height = 24
        Hint = 'Sound file to play when accepting an incoming connection'
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
        Left = 89
        Top = 44
        Width = 208
        Height = 24
        Hint = 'Sound file to play when disconnecting'
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
    Top = 247
    Width = 93
    Height = 31
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 128
    Top = 247
    Width = 92
    Height = 31
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object ApplyBtn: TButton
    Left = 233
    Top = 247
    Width = 93
    Height = 31
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
