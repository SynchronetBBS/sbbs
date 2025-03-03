object TelnetCfgDlg: TTelnetCfgDlg
  Left = 1133
  Top = 475
  BorderStyle = bsDialog
  Caption = 'Terminal Server Configuration'
  ClientHeight = 270
  ClientWidth = 330
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Microsoft Sans Serif'
  Font.Style = []
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 15
  object PageControl: TPageControl
    Left = 3
    Top = 3
    Width = 321
    Height = 215
    ActivePage = SshTabSheet
    TabIndex = 3
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object FirstNodeLabel: TLabel
        Left = 8
        Top = 12
        Width = 90
        Height = 23
        AutoSize = False
        Caption = 'First Node'
      end
      object LastNodeLabel: TLabel
        Left = 8
        Top = 42
        Width = 85
        Height = 23
        AutoSize = False
        Caption = 'Last Node'
      end
      object MaxConConLabel: TLabel
        Left = 8
        Top = 72
        Width = 85
        Height = 23
        AutoSize = False
        Caption = 'Max Con-Conn'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 171
        Top = 12
        Width = 135
        Height = 21
        Hint = 'Automatically start Terminal server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object FirstNodeEdit: TEdit
        Left = 98
        Top = 12
        Width = 45
        Height = 23
        Hint = 'First node number available for Terminal logins'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object XtrnMinCheckBox: TCheckBox
        Left = 171
        Top = 42
        Width = 135
        Height = 23
        Hint = 'External programs run in a minimized window'
        Caption = 'Minimize Externals'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object LastNodeEdit: TEdit
        Left = 98
        Top = 42
        Width = 45
        Height = 23
        Hint = 'Last node number available for Terminal logins'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object HostnameCheckBox: TCheckBox
        Left = 8
        Top = 132
        Width = 135
        Height = 26
        Hint = 'Automatically lookup client'#39's hostname via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object QWKEventsCheckBox: TCheckBox
        Left = 171
        Top = 102
        Width = 135
        Height = 23
        Hint = 'Handle QWK Message Packet Events in This Instance'
        Caption = 'QWK Msg Events'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object EventsCheckBox: TCheckBox
        Left = 171
        Top = 72
        Width = 135
        Height = 23
        Hint = 'Enable the events thread'
        Caption = 'Events Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object DosSupportCheckBox: TCheckBox
        Left = 8
        Top = 102
        Width = 150
        Height = 23
        Hint = 'Attempt to execute 16-bit DOS progarms (requires NTVDM)'
        Caption = 'DOS Program Support'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object MaxConConEdit: TEdit
        Left = 98
        Top = 72
        Width = 45
        Height = 23
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
        Left = 8
        Top = 42
        Width = 90
        Height = 24
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object TelnetPortLabel: TLabel
        Left = 8
        Top = 12
        Width = 90
        Height = 24
        AutoSize = False
        Caption = 'Listening Port'
      end
      object CmdLogCheckBox: TCheckBox
        Left = 8
        Top = 102
        Width = 195
        Height = 24
        Hint = 'Log (debug) all transmitted and received Telnet commands'
        Caption = 'Log Telnet Commands'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object TelnetInterfaceEdit: TEdit
        Left = 98
        Top = 42
        Width = 180
        Height = 23
        Hint = 
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object TelnetPortEdit: TEdit
        Left = 98
        Top = 12
        Width = 45
        Height = 23
        Hint = 'TCP port for incoming connections (default=23)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object TelnetGaCheckBox: TCheckBox
        Left = 8
        Top = 72
        Width = 195
        Height = 24
        Hint = 
          'Send periodic Telnet GA commands to help detect dropped connecti' +
          'ons'
        Caption = 'Send Telnet Go-Aheads'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object AutoLogonCheckBox: TCheckBox
        Left = 8
        Top = 132
        Width = 135
        Height = 21
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
        Left = 8
        Top = 12
        Width = 90
        Height = 24
        AutoSize = False
        Caption = 'Listening Port'
      end
      object RLoginInterfaceLabel: TLabel
        Left = 8
        Top = 42
        Width = 90
        Height = 24
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object RLoginPortEdit: TEdit
        Left = 98
        Top = 12
        Width = 45
        Height = 23
        Hint = 'TCP port for incoming connections (default=513)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object RLoginInterfaceEdit: TEdit
        Left = 98
        Top = 42
        Width = 180
        Height = 23
        Hint = 
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object RLoginEnabledCheckBox: TCheckBox
        Left = 212
        Top = 12
        Width = 85
        Height = 19
        Hint = 'Enable the RLogin port'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnClick = RLoginEnabledCheckBoxClick
      end
      object RLoginIPallowButton: TButton
        Left = 9
        Top = 72
        Width = 269
        Height = 24
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
        Left = 8
        Top = 12
        Width = 90
        Height = 24
        AutoSize = False
        Caption = 'Listening Port'
      end
      object SshInterfaceLabel: TLabel
        Left = 8
        Top = 42
        Width = 90
        Height = 24
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object SshConnectTimeoutLabel: TLabel
        Left = 8
        Top = 72
        Width = 90
        Height = 24
        AutoSize = False
        Caption = 'Conn Timeout'
      end
      object SFTPMaxInactivityLabel: TLabel
        Left = 8
        Top = 136
        Width = 90
        Height = 24
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object SshPortEdit: TEdit
        Left = 98
        Top = 12
        Width = 45
        Height = 23
        Hint = 'TCP port for incoming connections (default=22)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object SshEnabledCheckBox: TCheckBox
        Left = 212
        Top = 12
        Width = 85
        Height = 19
        Hint = 'Enable the Secure Shell (SSH) port'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnClick = SshEnabledCheckBoxClick
      end
      object SshInterfaceEdit: TEdit
        Left = 98
        Top = 42
        Width = 180
        Height = 23
        Hint = 
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object SshConnTimeoutEdit: TEdit
        Left = 98
        Top = 72
        Width = 45
        Height = 23
        Hint = 'SSH Connection Timeout'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object SFTPEnabledCheckBox: TCheckBox
        Left = 8
        Top = 102
        Width = 195
        Height = 24
        Hint = 'Enable SSH File Transfer (SFTP) support'
        Caption = 'File Transfer (SFTP) Support'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object SFTPMaxInactivityEdit: TEdit
        Left = 98
        Top = 136
        Width = 45
        Height = 23
        Hint = 'Maximum SFTP Session Inactivity'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 2
      object ConfigureSoundButton: TButton
        Left = 9
        Top = 9
        Width = 297
        Height = 29
        Caption = 'Configure Common Server Event Sounds'
        TabOrder = 0
        OnClick = ConfigureSoundButtonClick
      end
    end
  end
  object OKBtn: TButton
    Left = 23
    Top = 232
    Width = 88
    Height = 29
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 120
    Top = 232
    Width = 87
    Height = 29
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object ApplyBtn: TButton
    Left = 218
    Top = 232
    Width = 88
    Height = 29
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
