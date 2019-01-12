object WebCfgDlg: TWebCfgDlg
  Left = 492
  Top = 403
  BorderStyle = bsDialog
  Caption = 'Web Server Configuration'
  ClientHeight = 245
  ClientWidth = 286
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -10
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  ShowHint = True
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
    ActivePage = HttpTabSheet
    TabIndex = 2
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object MaxClientesLabel: TLabel
        Left = 7
        Top = 88
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 7
        Top = 114
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object PortLabel: TLabel
        Left = 7
        Top = 62
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Listening Port'
      end
      object InterfaceLabel: TLabel
        Left = 7
        Top = 36
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object AuthTypesLabel: TLabel
        Left = 7
        Top = 140
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Auth Types'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 117
        Height = 20
        Hint = 'Automatically start Web server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object MaxClientsEdit: TEdit
        Left = 85
        Top = 88
        Width = 39
        Height = 21
        Hint = 'Maximum number of simultaneous clients (0=unlimited)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object MaxInactivityEdit: TEdit
        Left = 85
        Top = 114
        Width = 39
        Height = 21
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=120)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object PortEdit: TEdit
        Left = 85
        Top = 62
        Width = 39
        Height = 21
        Hint = 'TCP port to use for HTTP connections (default=80)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object NetworkInterfaceEdit: TEdit
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
      object AuthTypesEdit: TEdit
        Left = 85
        Top = 140
        Width = 156
        Height = 21
        Hint = 'Comma-separated list of default authentication types allowed'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
    end
    object TlsTabSheet: TTabSheet
      Caption = 'TLS'
      ImageIndex = 5
      object TlsInterfaceLabel: TLabel
        Left = 7
        Top = 36
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object TlsPortLabel: TLabel
        Left = 7
        Top = 62
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Listening Port'
      end
      object TlsEnableCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 117
        Height = 20
        Hint = 'Enables HTTPS (HTTP over TLS/SSL)'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = TlsEnableCheckBoxClick
      end
      object TlsInterfaceEdit: TEdit
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
      object TlsPortEdit: TEdit
        Left = 85
        Top = 62
        Width = 39
        Height = 21
        Hint = 'TCP port to use for HTTPS connections (default=443)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
    end
    object HttpTabSheet: TTabSheet
      Caption = 'HTTP'
      ImageIndex = 3
      object HtmlDirLabel: TLabel
        Left = 7
        Top = 10
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'HTML Root'
      end
      object ErrorSubDirLabel: TLabel
        Left = 7
        Top = 36
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Error SubDir'
      end
      object ServerSideJsExtLabel: TLabel
        Left = 7
        Top = 114
        Width = 143
        Height = 20
        AutoSize = False
        Caption = 'Server-Side JS File Extension'
      end
      object IndexLabel: TLabel
        Left = 7
        Top = 62
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Index Files'
      end
      object HtmlRootEdit: TEdit
        Left = 85
        Top = 10
        Width = 156
        Height = 21
        Hint = 'Root directory for HTML files (off of CTRL directory)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object ErrorSubDirEdit: TEdit
        Left = 85
        Top = 36
        Width = 156
        Height = 21
        Hint = 'Error sub-directory (off of HTML root)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object ServerSideJsExtEdit: TEdit
        Left = 156
        Top = 114
        Width = 85
        Height = 21
        Hint = 
          'File extension that denotes server-side JavaScript files (e.g. "' +
          '.ssjs")'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object VirtualHostsCheckBox: TCheckBox
        Left = 7
        Top = 140
        Width = 85
        Height = 19
        Hint = 'Support virtual host directories off the HTML root directory'
        Caption = 'Virtual Hosts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object IndexFileEdit: TEdit
        Left = 85
        Top = 62
        Width = 156
        Height = 21
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
        Left = 7
        Top = 36
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'SubDirectory'
      end
      object CGIExtLabel: TLabel
        Left = 7
        Top = 62
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'File Extensions'
      end
      object CGIMaxInactivityLabel: TLabel
        Left = 7
        Top = 114
        Width = 78
        Height = 19
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object CGIContentLabel: TLabel
        Left = 7
        Top = 88
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Content-Type'
      end
      object CGIDirEdit: TEdit
        Left = 85
        Top = 36
        Width = 156
        Height = 21
        Hint = 'CGI sub-directory (off of HTML root)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object CGICheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 228
        Height = 20
        Hint = 'CGI support is enabled when checked'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = CGICheckBoxClick
      end
      object CGIExtEdit: TEdit
        Left = 85
        Top = 62
        Width = 156
        Height = 21
        Hint = 'File extensions that denote CGI executable files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object CGIMaxInactivityEdit: TEdit
        Left = 85
        Top = 114
        Width = 39
        Height = 21
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=120)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object CGIContentEdit: TEdit
        Left = 85
        Top = 88
        Width = 156
        Height = 21
        Hint = 'Default Content-Type for CGI output'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object CGIEnvButton: TButton
        Left = 7
        Top = 143
        Width = 117
        Height = 20
        Caption = 'Environment Vars'
        TabOrder = 5
        OnClick = CGIEnvButtonClick
      end
      object WebHandlersButton: TButton
        Left = 130
        Top = 143
        Width = 111
        Height = 20
        Caption = 'Content Handlers'
        TabOrder = 6
        OnClick = WebHandlersButtonClick
      end
    end
    object LogTabSheet: TTabSheet
      Caption = 'Log'
      ImageIndex = 1
      object LogBaseLabel: TLabel
        Left = 7
        Top = 88
        Width = 78
        Height = 20
        AutoSize = False
        Caption = 'Base Filename'
      end
      object DebugTxCheckBox: TCheckBox
        Left = 7
        Top = 34
        Width = 156
        Height = 20
        Hint = 'Log (debug) transmitted HTTP responses'
        Caption = 'Transmitted Responses'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DebugRxCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 156
        Height = 19
        Hint = 'Log (debug) all received HTTP requests'
        Caption = 'Received Requests'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object AccessLogCheckBox: TCheckBox
        Left = 7
        Top = 59
        Width = 156
        Height = 19
        Hint = 'Create HTTP access log files'
        Caption = 'Create Access Log Files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        OnClick = AccessLogCheckBoxClick
      end
      object LogBaseNameEdit: TEdit
        Left = 85
        Top = 88
        Width = 156
        Height = 21
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
        Left = 7
        Top = 10
        Width = 65
        Height = 20
        AutoSize = False
        Caption = 'Connect'
      end
      object HangupSoundLabel: TLabel
        Left = 7
        Top = 36
        Width = 65
        Height = 20
        AutoSize = False
        Caption = 'Disconnect'
      end
      object HackAttemptSoundLabel: TLabel
        Left = 7
        Top = 62
        Width = 65
        Height = 20
        AutoSize = False
        Caption = 'Hack Attempt'
      end
      object AnswerSoundEdit: TEdit
        Left = 85
        Top = 10
        Width = 156
        Height = 21
        Hint = 'Sound file to play when users connect'
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
        Left = 85
        Top = 36
        Width = 156
        Height = 21
        Hint = 'Sound file to play when users disconnect'
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
      object HackAttemptSoundEdit: TEdit
        Left = 85
        Top = 62
        Width = 156
        Height = 21
        Hint = 'Sound file to play when hack attempts are detected'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object HackAttemptSoundButton: TButton
        Left = 247
        Top = 62
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 5
        OnClick = HackAttemptSoundButtonClick
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
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Options = [ofHideReadOnly, ofNoChangeDir, ofEnableSizing, ofDontAddToRecent]
    Top = 240
  end
end
