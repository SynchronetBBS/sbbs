object FtpCfgDlg: TFtpCfgDlg
  Left = 466
  Top = 622
  BorderStyle = bsDialog
  Caption = 'FTP Server Configuration'
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
    ActivePage = SoundTabSheet
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object MaxClientesLabel: TLabel
        Left = 9
        Top = 102
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 9
        Top = 132
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object PortLabel: TLabel
        Left = 9
        Top = 68
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Control Port'
      end
      object InterfaceLabel: TLabel
        Left = 9
        Top = 36
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Interface (IP)'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 9
        Top = 6
        Width = 144
        Height = 24
        Hint = 'Automatically start FTP server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object MaxClientsEdit: TEdit
        Left = 105
        Top = 100
        Width = 48
        Height = 24
        Hint = 'Maximum number of simultaneous clients (default=10)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object MaxInactivityEdit: TEdit
        Left = 105
        Top = 132
        Width = 48
        Height = 24
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=300)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object PortEdit: TEdit
        Left = 105
        Top = 68
        Width = 48
        Height = 24
        Hint = 'TCP port to use for FTP control connections (default=21)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object NetworkInterfaceEdit: TEdit
        Left = 105
        Top = 36
        Width = 192
        Height = 24
        Hint = 'Your network adapter'#39's static IP address or blank for <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object AutoIndexCheckBox: TCheckBox
        Left = 9
        Top = 162
        Width = 96
        Height = 24
        Hint = 'Automatically generate index files for file descriptions'
        Caption = 'Auto Index'
        TabOrder = 6
        OnClick = AutoIndexCheckBoxClick
      end
      object IndexFileNameEdit: TEdit
        Left = 105
        Top = 164
        Width = 192
        Height = 24
        Hint = 'Name of auto-index file (default=00index)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object AllowQWKCheckBox: TCheckBox
        Left = 182
        Top = 68
        Width = 147
        Height = 24
        Hint = 'Allow QWK packet transfers'
        Caption = 'QWK Packets'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object LocalFileSysCheckBox: TCheckBox
        Left = 182
        Top = 100
        Width = 147
        Height = 24
        Hint = 
          'Allow sysop access to local file system (requires sysop password' +
          ')'
        Caption = 'Local File System'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
      end
      object HostnameCheckBox: TCheckBox
        Left = 182
        Top = 6
        Width = 147
        Height = 24
        Hint = 'Automatically lookup client'#39's hostnames via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DirFilesCheckBox: TCheckBox
        Left = 182
        Top = 129
        Width = 154
        Height = 25
        Hint = 'Allow users access to files in directory, but not in database'
        Caption = 'Directory File Access'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 10
      end
    end
    object LogTabSheet: TTabSheet
      Caption = 'Log'
      ImageIndex = 1
      object DebugTxCheckBox: TCheckBox
        Left = 9
        Top = 36
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
        Top = 6
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
        Top = 66
        Width = 192
        Height = 24
        Hint = 'Log (debug) data channel operations'
        Caption = 'Data Channel Activity'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object LogFileCheckBox: TCheckBox
        Left = 9
        Top = 96
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
      object HnagupSoundLabel: TLabel
        Left = 9
        Top = 44
        Width = 80
        Height = 25
        AutoSize = False
        Caption = 'Disconnect'
      end
      object AnswerSoundEdit: TEdit
        Left = 89
        Top = 12
        Width = 208
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
        Left = 89
        Top = 44
        Width = 208
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
    Top = 247
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
    Top = 247
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
    Top = 247
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
    Top = 240
  end
end
