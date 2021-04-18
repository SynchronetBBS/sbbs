object FtpCfgDlg: TFtpCfgDlg
  Left = 1021
  Top = 407
  BorderStyle = bsDialog
  Caption = 'FTP Server Configuration'
  ClientHeight = 245
  ClientWidth = 286
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    286
    245)
  PixelsPerInch = 96
  TextHeight = 13
  object PageControl: TPageControl
    Left = 3
    Top = 3
    Width = 278
    Height = 199
    ActivePage = SoundTabSheet
    TabIndex = 4
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object MaxClientesLabel: TLabel
        Left = 7
        Top = 86
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 7
        Top = 112
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object PortLabel: TLabel
        Left = 7
        Top = 60
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Control Port'
      end
      object InterfaceLabel: TLabel
        Left = 7
        Top = 34
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object QwkTimeoutLabel: TLabel
        Left = 148
        Top = 138
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'QWK Timeout'
      end
      object MaxConConLabel: TLabel
        Left = 7
        Top = 138
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Max Con-Conn'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 117
        Height = 20
        Hint = 'Automatically start FTP server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object MaxClientsEdit: TEdit
        Left = 85
        Top = 86
        Width = 39
        Height = 21
        Hint = 'Maximum number of simultaneous clients (default=10)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object MaxInactivityEdit: TEdit
        Left = 85
        Top = 112
        Width = 39
        Height = 21
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=300)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object PortEdit: TEdit
        Left = 85
        Top = 60
        Width = 39
        Height = 21
        Hint = 'TCP port to use for FTP control connections (default=21)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object NetworkInterfaceEdit: TEdit
        Left = 85
        Top = 34
        Width = 156
        Height = 21
        Hint = 
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object AllowQWKCheckBox: TCheckBox
        Left = 148
        Top = 109
        Width = 119
        Height = 20
        Hint = 'Allow QWK packet transfers'
        Caption = 'QWK Packets'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
        OnClick = AllowQWKCheckBoxClick
      end
      object LocalFileSysCheckBox: TCheckBox
        Left = 148
        Top = 62
        Width = 119
        Height = 20
        Hint = 
          'Allow sysop access to local file system (requires sysop password' +
          ')'
        Caption = 'Local File System'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object HostnameCheckBox: TCheckBox
        Left = 148
        Top = 10
        Width = 119
        Height = 20
        Hint = 'Automatically lookup client'#39's hostnames via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object AllowBounceCheckBox: TCheckBox
        Left = 148
        Top = 86
        Width = 125
        Height = 20
        Hint = 'Allow authenticated users to use FTP Bounce (not recommended)'
        Caption = 'Allow Bouncing'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object QwkTimeoutEdit: TEdit
        Left = 223
        Top = 138
        Width = 39
        Height = 21
        Hint = 'Maximum number of seconds before QWK packet creation timeout'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 10
      end
      object MaxConConEdit: TEdit
        Left = 85
        Top = 138
        Width = 39
        Height = 21
        Hint = 'Maximum Concurrent Connections from same IP (0=unlimited)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
    end
    object PasvTabSheet: TTabSheet
      Caption = 'Passive'
      ImageIndex = 4
      object PasvIpLabel: TLabel
        Left = 7
        Top = 10
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'IPv4 Address'
      end
      object PasvPortLabel: TLabel
        Left = 7
        Top = 36
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Port Range'
      end
      object PasvPortThroughLabel: TLabel
        Left = 124
        Top = 36
        Width = 13
        Height = 20
        Alignment = taCenter
        AutoSize = False
        Caption = '-'
      end
      object PasvIPv4AddrEdit: TEdit
        Left = 85
        Top = 10
        Width = 92
        Height = 21
        Hint = 'Your static public IP address or blank for <unspecified>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object PasvPortLowEdit: TEdit
        Left = 85
        Top = 36
        Width = 39
        Height = 21
        Hint = 
          'Lowest TCP port to use for passive FTP data connections (default' +
          '=1024)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object PasvPortHighEdit: TEdit
        Left = 137
        Top = 36
        Width = 39
        Height = 21
        Hint = 
          'Highest TCP port to use for passive FTP data connections (defaul' +
          't=65535)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object PasvIpLookupCheckBox: TCheckBox
        Left = 184
        Top = 8
        Width = 73
        Height = 17
        Hint = 'Get passive IP address from public hostname (dynamic IP)'
        Caption = 'Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnClick = PasvIpLookupCheckBoxClick
      end
    end
    object IndexTabSheet: TTabSheet
      Caption = 'Index'
      ImageIndex = 3
      object AutoIndexCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 78
        Height = 19
        Hint = 'Automatically generate index files for file descriptions'
        Caption = 'ASCII'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = AutoIndexCheckBoxClick
      end
      object IndexFileNameEdit: TEdit
        Left = 85
        Top = 10
        Width = 156
        Height = 21
        Hint = 'Name of ASCII index file (default=00index)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
    end
    object LogTabSheet: TTabSheet
      Caption = 'Log'
      ImageIndex = 1
      object DebugTxCheckBox: TCheckBox
        Left = 7
        Top = 34
        Width = 156
        Height = 20
        Hint = 'Log (debug) transmitted FTP responses'
        Caption = 'Transmitted Responses'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object CmdLogCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 156
        Height = 19
        Hint = 'Log (debug) all received FTP commands'
        Caption = 'Received Commands'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object DebugDataCheckBox: TCheckBox
        Left = 7
        Top = 59
        Width = 156
        Height = 19
        Hint = 'Log (debug) data channel operations'
        Caption = 'Data Channel Activity'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object LogFileCheckBox: TCheckBox
        Left = 7
        Top = 83
        Width = 117
        Height = 20
        Hint = 'Save log entries to a file (in your DATA directory)'
        Caption = 'Log to Disk'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 2
      object ConfigureSoundButton: TButton
        Left = 8
        Top = 8
        Width = 257
        Height = 25
        Caption = 'Configure Common Server Event Sounds'
        TabOrder = 0
        OnClick = ConfigureSoundButtonClick
      end
    end
  end
  object OKBtn: TButton
    Left = 20
    Top = 211
    Width = 76
    Height = 25
    Anchors = [akLeft, akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 104
    Top = 211
    Width = 75
    Height = 25
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object ApplyBtn: TButton
    Left = 189
    Top = 211
    Width = 76
    Height = 25
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Apply'
    TabOrder = 3
    OnClick = OKBtnClick
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Options = [ofHideReadOnly, ofNoChangeDir, ofEnableSizing, ofDontAddToRecent]
    Top = 240
  end
end
