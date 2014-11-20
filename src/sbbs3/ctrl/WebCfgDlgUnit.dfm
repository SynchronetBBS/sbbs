object WebCfgDlg: TWebCfgDlg
  Left = 492
  Top = 403
  BorderStyle = bsDialog
  Caption = 'Web Server Configuration'
  ClientHeight = 302
  ClientWidth = 352
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -13
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  ShowHint = True
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
    ActivePage = HttpTabSheet
    TabIndex = 1
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object MaxClientesLabel: TLabel
        Left = 9
        Top = 108
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 9
        Top = 140
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object PortLabel: TLabel
        Left = 9
        Top = 76
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Listening Port'
      end
      object InterfaceLabel: TLabel
        Left = 9
        Top = 44
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Interface (IP)'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 144
        Height = 25
        Hint = 'Automatically start Web server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object MaxClientsEdit: TEdit
        Left = 105
        Top = 108
        Width = 48
        Height = 24
        Hint = 'Maximum number of simultaneous clients (0=unlimited)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object MaxInactivityEdit: TEdit
        Left = 105
        Top = 140
        Width = 48
        Height = 24
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=120)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object PortEdit: TEdit
        Left = 105
        Top = 76
        Width = 48
        Height = 24
        Hint = 'TCP port to use for HTTP connections (default=80)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object NetworkInterfaceEdit: TEdit
        Left = 105
        Top = 44
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
    end
    object HttpTabSheet: TTabSheet
      Caption = 'HTTP'
      ImageIndex = 3
      object HtmlDirLabel: TLabel
        Left = 9
        Top = 12
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'HTML Root'
      end
      object ErrorSubDirLabel: TLabel
        Left = 9
        Top = 44
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'Error SubDir'
      end
      object ServerSideJsExtLabel: TLabel
        Left = 9
        Top = 140
        Width = 176
        Height = 25
        AutoSize = False
        Caption = 'Server-Side JS File Extension'
      end
      object EmbeddedJsExtLabel: TLabel
        Left = 9
        Top = 108
        Width = 176
        Height = 25
        AutoSize = False
        Caption = 'Embedded JS File Extension'
        Enabled = False
        Visible = False
      end
      object IndexLabel: TLabel
        Left = 9
        Top = 76
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'Index Files'
      end
      object HtmlRootEdit: TEdit
        Left = 105
        Top = 12
        Width = 192
        Height = 24
        Hint = 'Root directory for HTML files (off of CTRL directory)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object ErrorSubDirEdit: TEdit
        Left = 105
        Top = 44
        Width = 192
        Height = 24
        Hint = 'Error sub-directory (off of HTML root)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object ServerSideJsExtEdit: TEdit
        Left = 192
        Top = 140
        Width = 105
        Height = 24
        Hint = 
          'File extension that denotes server-side JavaScript files (e.g. "' +
          '.ssjs")'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object EmbeddedJsExtEdit: TEdit
        Left = 192
        Top = 108
        Width = 105
        Height = 24
        Hint = 
          'File extension that denotes embedded JavaScript files (e.g. ".bb' +
          's")'
        Enabled = False
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
        Visible = False
      end
      object VirtualHostsCheckBox: TCheckBox
        Left = 9
        Top = 172
        Width = 104
        Height = 24
        Hint = 'Support virtual host directories off the HTML root directory'
        Caption = 'Virtual Hosts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object IndexFileEdit: TEdit
        Left = 104
        Top = 76
        Width = 193
        Height = 24
        Hint = 
          'List of filenames that will be automatically sent to client (e.g' +
          '. index.html)'
        TabOrder = 2
      end
    end
    object CGITabSheet: TTabSheet
      Caption = 'CGI'
      ImageIndex = 4
      object CGIDirLabel: TLabel
        Left = 9
        Top = 44
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'SubDirectory'
      end
      object CGIExtLabel: TLabel
        Left = 9
        Top = 76
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'File Extensions'
      end
      object CGIMaxInactivityLabel: TLabel
        Left = 9
        Top = 140
        Width = 96
        Height = 24
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object CGIContentLabel: TLabel
        Left = 9
        Top = 108
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'Content-Type'
      end
      object CGIDirEdit: TEdit
        Left = 105
        Top = 44
        Width = 192
        Height = 24
        Hint = 'CGI sub-directory (off of HTML root)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object CGICheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 280
        Height = 25
        Hint = 'CGI support is enabled when checked'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = CGICheckBoxClick
      end
      object CGIExtEdit: TEdit
        Left = 105
        Top = 76
        Width = 192
        Height = 24
        Hint = 'File extensions that denote CGI executable files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object CGIMaxInactivityEdit: TEdit
        Left = 105
        Top = 140
        Width = 48
        Height = 24
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=120)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object CGIContentEdit: TEdit
        Left = 105
        Top = 108
        Width = 192
        Height = 24
        Hint = 'Default Content-Type for CGI output'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object CGIEnvButton: TButton
        Left = 8
        Top = 176
        Width = 145
        Height = 25
        Caption = 'Environment Vars'
        TabOrder = 5
        OnClick = CGIEnvButtonClick
      end
      object WebHandlersButton: TButton
        Left = 160
        Top = 176
        Width = 137
        Height = 25
        Caption = 'Content Handlers'
        TabOrder = 6
        OnClick = WebHandlersButtonClick
      end
    end
    object LogTabSheet: TTabSheet
      Caption = 'Log'
      ImageIndex = 1
      object LogBaseLabel: TLabel
        Left = 9
        Top = 108
        Width = 96
        Height = 25
        AutoSize = False
        Caption = 'Base Filename'
      end
      object DebugTxCheckBox: TCheckBox
        Left = 9
        Top = 42
        Width = 192
        Height = 24
        Hint = 'Log (debug) transmitted HTTP responses'
        Caption = 'Transmitted Responses'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DebugRxCheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 192
        Height = 24
        Hint = 'Log (debug) all received HTTP requests'
        Caption = 'Received Requests'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object AccessLogCheckBox: TCheckBox
        Left = 9
        Top = 73
        Width = 192
        Height = 23
        Hint = 'Create HTTP access log files'
        Caption = 'Create Access Log Files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        OnClick = AccessLogCheckBoxClick
      end
      object LogBaseNameEdit: TEdit
        Left = 105
        Top = 108
        Width = 192
        Height = 24
        Hint = 'Base directory and filename for HTTP access log files'
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
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Options = [ofHideReadOnly, ofNoChangeDir, ofEnableSizing, ofDontAddToRecent]
    Top = 240
  end
end
