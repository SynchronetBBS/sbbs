object FtpCfgDlg: TFtpCfgDlg
  Left = 779
  Top = 375
  BorderStyle = bsDialog
  Caption = 'FTP Server Configuration'
  ClientHeight = 302
  ClientWidth = 352
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    352
    302)
  PixelsPerInch = 120
  TextHeight = 16
  object PageControl: TPageControl
    Left = 4
    Top = 4
    Width = 342
    Height = 245
    ActivePage = PasvTabSheet
    TabIndex = 1
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object MaxClientesLabel: TLabel
        Left = 9
        Top = 106
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 9
        Top = 138
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object PortLabel: TLabel
        Left = 9
        Top = 74
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Control Port'
      end
      object InterfaceLabel: TLabel
        Left = 9
        Top = 42
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Interface (IP)'
      end
      object QwkTimeoutLabel: TLabel
        Left = 9
        Top = 170
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'QWK Timeout'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 144
        Height = 25
        Hint = 'Automatically start FTP server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object MaxClientsEdit: TEdit
        Left = 105
        Top = 106
        Width = 48
        Height = 21
        Hint = 'Maximum number of simultaneous clients (default=10)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object MaxInactivityEdit: TEdit
        Left = 105
        Top = 138
        Width = 48
        Height = 21
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=300)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object PortEdit: TEdit
        Left = 105
        Top = 74
        Width = 48
        Height = 21
        Hint = 'TCP port to use for FTP control connections (default=21)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object NetworkInterfaceEdit: TEdit
        Left = 105
        Top = 42
        Width = 192
        Height = 21
        Hint = 'Your network adapter'#39's static IP address or blank for <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object AllowQWKCheckBox: TCheckBox
        Left = 182
        Top = 74
        Width = 147
        Height = 24
        Hint = 'Allow QWK packet transfers'
        Caption = 'QWK Packets'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object LocalFileSysCheckBox: TCheckBox
        Left = 182
        Top = 106
        Width = 147
        Height = 24
        Hint = 
          'Allow sysop access to local file system (requires sysop password' +
          ')'
        Caption = 'Local File System'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
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
      object DirFilesCheckBox: TCheckBox
        Left = 182
        Top = 135
        Width = 154
        Height = 25
        Hint = 'Allow users access to files in directory, but not in database'
        Caption = 'Directory File Access'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
      end
      object QwkTimeoutEdit: TEdit
        Left = 105
        Top = 170
        Width = 48
        Height = 21
        Hint = 'Maximum number of seconds before QWK packet creation timeout'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
    end
    object PasvTabSheet: TTabSheet
      Caption = 'Passive'
      ImageIndex = 4
      object PasvIpLabel: TLabel
        Left = 9
        Top = 12
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'IP Address'
      end
      object PasvPortLabel: TLabel
        Left = 9
        Top = 44
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'Port Range'
      end
      object PasvPortThroughLabel: TLabel
        Left = 153
        Top = 44
        Width = 16
        Height = 25
        Alignment = taCenter
        AutoSize = False
        Caption = '-'
      end
      object PasvIpAddrEdit: TEdit
        Left = 105
        Top = 12
        Width = 113
        Height = 24
        Hint = 'Your static public IP address or blank for <unspecified>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object PasvPortLowEdit: TEdit
        Left = 105
        Top = 44
        Width = 48
        Height = 24
        Hint = 
          'Lowest TCP port to use for passive FTP data connections (default' +
          '=1024)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object PasvPortHighEdit: TEdit
        Left = 169
        Top = 44
        Width = 48
        Height = 24
        Hint = 
          'Highest TCP port to use for passive FTP data connections (defaul' +
          't=65535)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object PasvIpLookupCheckBox: TCheckBox
        Left = 226
        Top = 10
        Width = 90
        Height = 21
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
      object HtmlJavaScriptLabel: TLabel
        Left = 9
        Top = 76
        Width = 80
        Height = 25
        AutoSize = False
        Caption = 'JavaScript'
      end
      object AutoIndexCheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 96
        Height = 24
        Hint = 'Automatically generate index files for file descriptions'
        Caption = 'ASCII'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = AutoIndexCheckBoxClick
      end
      object IndexFileNameEdit: TEdit
        Left = 105
        Top = 12
        Width = 192
        Height = 21
        Hint = 'Name of ASCII index file (default=00index)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object HtmlIndexCheckBox: TCheckBox
        Left = 9
        Top = 44
        Width = 96
        Height = 24
        Hint = 'Automatically generate HTML index files for file descriptions'
        Caption = 'HTML'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        OnClick = HtmlIndexCheckBoxClick
      end
      object HtmlFileNameEdit: TEdit
        Left = 105
        Top = 44
        Width = 192
        Height = 21
        Hint = 'Name of HTML index file (default=00index.html)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object HtmlJavaScriptEdit: TEdit
        Left = 105
        Top = 76
        Width = 192
        Height = 21
        Hint = 'JavaScript filename to execute to generate HTML index file'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
    end
    object LogTabSheet: TTabSheet
      Caption = 'Log'
      ImageIndex = 1
      object DebugTxCheckBox: TCheckBox
        Left = 9
        Top = 42
        Width = 192
        Height = 24
        Hint = 'Log (debug) transmitted FTP responses'
        Caption = 'Transmitted Responses'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object CmdLogCheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 192
        Height = 24
        Hint = 'Log (debug) all received FTP commands'
        Caption = 'Received Commands'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object DebugDataCheckBox: TCheckBox
        Left = 9
        Top = 73
        Width = 192
        Height = 23
        Hint = 'Log (debug) data channel operations'
        Caption = 'Data Channel Activity'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object LogFileCheckBox: TCheckBox
        Left = 9
        Top = 102
        Width = 144
        Height = 25
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
      object HackAttemptSoundLabel: TLabel
        Left = 9
        Top = 76
        Width = 80
        Height = 25
        AutoSize = False
        Caption = 'Hack Attempt'
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
      object HackAttemptSoundEdit: TEdit
        Left = 105
        Top = 76
        Width = 192
        Height = 24
        Hint = 'Sound file to play when hack attempts are detected'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object HackAttemptSoundButton: TButton
        Left = 304
        Top = 76
        Width = 25
        Height = 26
        Caption = '...'
        TabOrder = 5
        OnClick = HackAttemptSoundButtonClick
      end
    end
  end
  object OKBtn: TButton
    Left = 25
    Top = 260
    Width = 93
    Height = 30
    Anchors = [akLeft, akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 128
    Top = 260
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
    Top = 260
    Width = 93
    Height = 30
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
