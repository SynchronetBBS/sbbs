object FtpCfgDlg: TFtpCfgDlg
  Left = 812
  Top = 620
  BorderStyle = bsDialog
  Caption = 'FTP Server Configuration'
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
      object MaxClientesLabel: TLabel
        Left = 7
        Top = 83
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 7
        Top = 107
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object PortLabel: TLabel
        Left = 7
        Top = 55
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Control Port'
      end
      object InterfaceLabel: TLabel
        Left = 7
        Top = 29
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Interface (IP)'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 7
        Top = 5
        Width = 117
        Height = 19
        Hint = 'Automatically start'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object MaxClientsEdit: TEdit
        Left = 85
        Top = 81
        Width = 39
        Height = 21
        Hint = 'Maximum number of simultaneous clients (default=10)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object MaxInactivityEdit: TEdit
        Left = 85
        Top = 107
        Width = 39
        Height = 21
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=300)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object PortEdit: TEdit
        Left = 85
        Top = 55
        Width = 39
        Height = 21
        Hint = 'FTP control port (default=21)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object NetworkInterfaceEdit: TEdit
        Left = 85
        Top = 29
        Width = 156
        Height = 21
        Hint = 'Your network adapter'#39's static IP address or blank for <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object AutoIndexCheckBox: TCheckBox
        Left = 7
        Top = 132
        Width = 78
        Height = 19
        Hint = 'Automatically generate index files for file descriptions'
        Caption = 'Auto Index'
        TabOrder = 5
        OnClick = AutoIndexCheckBoxClick
      end
      object IndexFileNameEdit: TEdit
        Left = 85
        Top = 133
        Width = 156
        Height = 21
        Hint = 'Name of auto-index file (default=00index)'
        TabOrder = 6
      end
      object AllowQWKCheckBox: TCheckBox
        Left = 148
        Top = 55
        Width = 119
        Height = 20
        Hint = 'Allow QWK packet transfers'
        Caption = 'QWK Packets'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object LocalFileSysCheckBox: TCheckBox
        Left = 148
        Top = 81
        Width = 119
        Height = 20
        Hint = 'Allow sysop access to local file system'
        Caption = 'Local File System'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object HostnameCheckBox: TCheckBox
        Left = 148
        Top = 5
        Width = 119
        Height = 19
        Hint = 'Automatically lookup client'#39's hostnames via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
      end
      object DirFilesCheckBox: TCheckBox
        Left = 148
        Top = 105
        Width = 125
        Height = 20
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
        Left = 7
        Top = 29
        Width = 156
        Height = 20
        Hint = 'Log (debug) transmitted FTP responses'
        Caption = 'Transmitted Responses'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object CmdLogCheckBox: TCheckBox
        Left = 7
        Top = 5
        Width = 156
        Height = 19
        Hint = 'Log (debug) all received FTP commands'
        Caption = 'Received Commands'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DebugDataCheckBox: TCheckBox
        Left = 7
        Top = 54
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
        Top = 78
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
      object AnswerSoundLabel: TLabel
        Left = 7
        Top = 11
        Width = 65
        Height = 20
        AutoSize = False
        Caption = 'Connect'
      end
      object HnagupSoundLabel: TLabel
        Left = 7
        Top = 37
        Width = 65
        Height = 20
        AutoSize = False
        Caption = 'Disconnect'
      end
      object AnswerSoundEdit: TEdit
        Left = 72
        Top = 11
        Width = 169
        Height = 21
        Hint = 'WAV file to play when users connect'
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
        Hint = 'WAV file to play when users disconnect'
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
    Height = 24
    Anchors = [akLeft, akBottom]
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
    Height = 24
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object ApplyBtn: TButton
    Left = 189
    Top = 201
    Width = 76
    Height = 24
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
