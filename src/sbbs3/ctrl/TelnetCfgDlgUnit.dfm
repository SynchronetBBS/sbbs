object TelnetCfgDlg: TTelnetCfgDlg
  Left = 1133
  Top = 475
  BorderStyle = bsDialog
  Caption = 'Terminal Server Configuration'
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
    TabIndex = 0
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object FirstNodeLabel: TLabel
        Left = 7
        Top = 10
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'First Node'
      end
      object LastNodeLabel: TLabel
        Left = 7
        Top = 36
        Width = 74
        Height = 20
        AutoSize = False
        Caption = 'Last Node'
      end
      object MaxConConLabel: TLabel
        Left = 7
        Top = 62
        Width = 74
        Height = 20
        AutoSize = False
        Caption = 'Max Con-Conn'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 148
        Top = 10
        Width = 117
        Height = 19
        Hint = 'Automatically start Terminal server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object FirstNodeEdit: TEdit
        Left = 85
        Top = 10
        Width = 39
        Height = 21
        Hint = 'First node number available for Terminal logins'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object XtrnMinCheckBox: TCheckBox
        Left = 148
        Top = 36
        Width = 117
        Height = 20
        Hint = 'External programs run in a minimized window'
        Caption = 'Minimize Externals'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object LastNodeEdit: TEdit
        Left = 85
        Top = 36
        Width = 39
        Height = 21
        Hint = 'Last node number available for Terminal logins'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object HostnameCheckBox: TCheckBox
        Left = 7
        Top = 114
        Width = 117
        Height = 23
        Hint = 'Automatically lookup client'#39's hostname via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object QWKEventsCheckBox: TCheckBox
        Left = 148
        Top = 88
        Width = 117
        Height = 20
        Hint = 'Handle QWK Message Packet Events in This Instance'
        Caption = 'QWK Msg Events'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object EventsCheckBox: TCheckBox
        Left = 148
        Top = 62
        Width = 117
        Height = 20
        Hint = 'Enable the events thread'
        Caption = 'Events Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object DosSupportCheckBox: TCheckBox
        Left = 7
        Top = 88
        Width = 130
        Height = 20
        Hint = 'Attempt to execute DOS progarms (requires 32-bit OS)'
        Caption = 'DOS Program Support'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object MaxConConEdit: TEdit
        Left = 85
        Top = 62
        Width = 39
        Height = 21
        Hint = 
          'Maximum unauthenticated Concurrent Connections from same IP (0=u' +
          'nlimited)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
    end
    object TelnetTabSheet: TTabSheet
      Caption = 'Telnet'
      ImageIndex = 1
      object InterfaceLabel: TLabel
        Left = 7
        Top = 36
        Width = 78
        Height = 21
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object TelnetPortLabel: TLabel
        Left = 7
        Top = 10
        Width = 78
        Height = 21
        AutoSize = False
        Caption = 'Listening Port'
      end
      object CmdLogCheckBox: TCheckBox
        Left = 7
        Top = 88
        Width = 169
        Height = 21
        Hint = 'Log (debug) all transmitted and received Telnet commands'
        Caption = 'Log Telnet Commands'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object TelnetInterfaceEdit: TEdit
        Left = 85
        Top = 36
        Width = 156
        Height = 21
        Hint = 
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object TelnetPortEdit: TEdit
        Left = 85
        Top = 10
        Width = 39
        Height = 21
        Hint = 'TCP port for incoming connections (default=23)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object TelnetGaCheckBox: TCheckBox
        Left = 7
        Top = 62
        Width = 169
        Height = 21
        Hint = 
          'Send periodic Telnet GA commands to help detect dropped connecti' +
          'ons'
        Caption = 'Send Telnet Go-Aheads'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object AutoLogonCheckBox: TCheckBox
        Left = 7
        Top = 114
        Width = 117
        Height = 19
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
        Left = 7
        Top = 10
        Width = 78
        Height = 21
        AutoSize = False
        Caption = 'Listening Port'
      end
      object RLoginInterfaceLabel: TLabel
        Left = 7
        Top = 36
        Width = 78
        Height = 21
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object RLoginPortEdit: TEdit
        Left = 85
        Top = 10
        Width = 39
        Height = 21
        Hint = 'TCP port for incoming connections (default=513)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object RLoginInterfaceEdit: TEdit
        Left = 85
        Top = 36
        Width = 156
        Height = 21
        Hint = 
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object RLoginEnabledCheckBox: TCheckBox
        Left = 184
        Top = 10
        Width = 73
        Height = 17
        Hint = 'Enable the RLogin port'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnClick = RLoginEnabledCheckBoxClick
      end
      object RLoginIPallowButton: TButton
        Left = 8
        Top = 62
        Width = 233
        Height = 21
        Hint = 
          'IP addresses of trusted hosts to allow unauthenticed RLogins fro' +
          'm'
        Caption = 'Allow Unauthenticated Logins from these IPs'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
        OnClick = RLoginIPallowButtonClick
      end
    end
    object SshTabSheet: TTabSheet
      Caption = 'SSH'
      ImageIndex = 4
      object SshPortLabel: TLabel
        Left = 7
        Top = 10
        Width = 78
        Height = 21
        AutoSize = False
        Caption = 'Listening Port'
      end
      object SshInterfaceLabel: TLabel
        Left = 7
        Top = 36
        Width = 78
        Height = 21
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object SshConnectTimeoutLabel: TLabel
        Left = 7
        Top = 62
        Width = 78
        Height = 21
        AutoSize = False
        Caption = 'Conn Timeout'
      end
      object SshPortEdit: TEdit
        Left = 85
        Top = 10
        Width = 39
        Height = 21
        Hint = 'TCP port for incoming connections (default=22)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object SshEnabledCheckBox: TCheckBox
        Left = 184
        Top = 10
        Width = 73
        Height = 17
        Hint = 'Enable the Secure Shell (SSH) port'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnClick = SshEnabledCheckBoxClick
      end
      object SshInterfaceEdit: TEdit
        Left = 85
        Top = 36
        Width = 156
        Height = 21
        Hint = 
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object SshConnTimeoutEdit: TEdit
        Left = 85
        Top = 62
        Width = 39
        Height = 21
        Hint = 'SSH Connection Timeout (in seconds)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 2
      object AnswerSoundLabel: TLabel
        Left = 7
        Top = 10
        Width = 65
        Height = 20
        AutoSize = False
        Caption = 'Connect'
      end
      object HnagupSoundLabel: TLabel
        Left = 7
        Top = 36
        Width = 65
        Height = 20
        AutoSize = False
        Caption = 'Disconnect'
      end
      object AnswerSoundEdit: TEdit
        Left = 72
        Top = 10
        Width = 169
        Height = 21
        Hint = 'Sound file to play when accepting an incoming connection'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object AnswerSoundButton: TButton
        Left = 247
        Top = 10
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 1
        OnClick = AnswerSoundButtonClick
      end
      object HangupSoundEdit: TEdit
        Left = 72
        Top = 36
        Width = 169
        Height = 21
        Hint = 'Sound file to play when disconnecting'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object HangupSoundButton: TButton
        Left = 247
        Top = 36
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
    Options = [ofHideReadOnly, ofNoChangeDir, ofEnableSizing, ofDontAddToRecent]
    Top = 248
  end
end
