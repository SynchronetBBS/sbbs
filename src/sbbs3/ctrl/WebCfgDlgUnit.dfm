object WebCfgDlg: TWebCfgDlg
  Left = 856
  Top = 332
  BorderStyle = bsDialog
  Caption = 'Web Server Configuration'
  ClientHeight = 283
  ClientWidth = 330
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Microsoft Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  ShowHint = True
  OnShow = FormShow
  DesignSize = (
    330
    283)
  PixelsPerInch = 96
  TextHeight = 15
  object PageControl: TPageControl
    Left = 3
    Top = 3
    Width = 321
    Height = 230
    ActivePage = HttpTabSheet
    TabIndex = 2
    TabOrder = 0
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object MaxClientesLabel: TLabel
        Left = 8
        Top = 102
        Width = 90
        Height = 21
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 8
        Top = 132
        Width = 90
        Height = 21
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object PortLabel: TLabel
        Left = 8
        Top = 72
        Width = 90
        Height = 21
        AutoSize = False
        Caption = 'Listening Port'
      end
      object InterfaceLabel: TLabel
        Left = 8
        Top = 42
        Width = 90
        Height = 21
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object MaxConConLabel: TLabel
        Left = 8
        Top = 159
        Width = 89
        Height = 22
        AutoSize = False
        Caption = 'Max Con-Conn'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 8
        Top = 12
        Width = 135
        Height = 23
        Hint = 'Automatically start Web server'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object MaxClientsEdit: TEdit
        Left = 98
        Top = 102
        Width = 45
        Height = 23
        Hint = 'Maximum number of simultaneous clients (0=unlimited)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object MaxInactivityEdit: TEdit
        Left = 98
        Top = 132
        Width = 45
        Height = 23
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=120)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object PortEdit: TEdit
        Left = 98
        Top = 72
        Width = 45
        Height = 23
        Hint = 'TCP port to use for HTTP connections (default=80)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object NetworkInterfaceEdit: TEdit
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
      object HostnameCheckBox: TCheckBox
        Left = 171
        Top = 12
        Width = 137
        Height = 23
        Hint = 'Automatically lookup client'#39's hostnames via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object MaxConConEdit: TEdit
        Left = 98
        Top = 159
        Width = 45
        Height = 23
        Hint = 'Maximum Concurrent Connections from same IP (0=unlimited)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
    end
    object TlsTabSheet: TTabSheet
      Caption = 'TLS'
      ImageIndex = 5
      object TlsInterfaceLabel: TLabel
        Left = 8
        Top = 42
        Width = 90
        Height = 21
        AutoSize = False
        Caption = 'Interfaces (IPs)'
      end
      object TlsPortLabel: TLabel
        Left = 8
        Top = 72
        Width = 90
        Height = 21
        AutoSize = False
        Caption = 'Listening Port'
      end
      object TlsEnableCheckBox: TCheckBox
        Left = 8
        Top = 12
        Width = 135
        Height = 23
        Hint = 'Enables HTTPS (HTTP over TLS/SSL)'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = TlsEnableCheckBoxClick
      end
      object TlsInterfaceEdit: TEdit
        Left = 98
        Top = 42
        Width = 180
        Height = 21
        Hint = 
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object TlsPortEdit: TEdit
        Left = 98
        Top = 72
        Width = 45
        Height = 21
        Hint = 'TCP port to use for HTTPS connections (default=443)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object HSTSEnableCheckBox: TCheckBox
        Left = 171
        Top = 12
        Width = 137
        Height = 23
        Hint = 'HTTP Strict Transport Security'
        Caption = 'HSTS Support'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
    end
    object HttpTabSheet: TTabSheet
      Caption = 'HTTP'
      ImageIndex = 3
      object HtmlDirLabel: TLabel
        Left = 8
        Top = 12
        Width = 90
        Height = 23
        AutoSize = False
        Caption = 'HTML Root'
      end
      object ErrorSubDirLabel: TLabel
        Left = 8
        Top = 42
        Width = 90
        Height = 23
        AutoSize = False
        Caption = 'Error SubDir'
      end
      object ServerSideJsExtLabel: TLabel
        Left = 8
        Top = 132
        Width = 165
        Height = 23
        AutoSize = False
        Caption = 'Server-Side JS File Extension'
      end
      object IndexLabel: TLabel
        Left = 8
        Top = 72
        Width = 90
        Height = 23
        AutoSize = False
        Caption = 'Index Files'
      end
      object AuthTypesLabel: TLabel
        Left = 8
        Top = 102
        Width = 90
        Height = 21
        AutoSize = False
        Caption = 'Auth Types'
      end
      object HtmlRootEdit: TEdit
        Left = 98
        Top = 12
        Width = 180
        Height = 23
        Hint = 'Root directory for HTML files (off of CTRL directory)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object ErrorSubDirEdit: TEdit
        Left = 98
        Top = 42
        Width = 180
        Height = 23
        Hint = 'Error sub-directory (off of HTML root)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object ServerSideJsExtEdit: TEdit
        Left = 180
        Top = 132
        Width = 98
        Height = 23
        Hint = 
          'File extension that denotes server-side JavaScript files (e.g. "' +
          '.ssjs")'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object VirtualHostsCheckBox: TCheckBox
        Left = 8
        Top = 162
        Width = 98
        Height = 21
        Hint = 'Support virtual host directories off the HTML root directory'
        Caption = 'Virtual Hosts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object IndexFileEdit: TEdit
        Left = 98
        Top = 72
        Width = 180
        Height = 23
        Hint = 
          'List of filenames that will be automatically sent to client (e.g' +
          '. index.html)'
        TabOrder = 2
      end
      object AuthTypesEdit: TEdit
        Left = 98
        Top = 102
        Width = 180
        Height = 23
        Hint = 'Comma-separated list of default authentication types allowed'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
    end
    object CGITabSheet: TTabSheet
      Caption = 'CGI'
      ImageIndex = 4
      object CGIDirLabel: TLabel
        Left = 8
        Top = 42
        Width = 90
        Height = 23
        AutoSize = False
        Caption = 'SubDirectory'
      end
      object CGIExtLabel: TLabel
        Left = 8
        Top = 72
        Width = 90
        Height = 23
        AutoSize = False
        Caption = 'File Extensions'
      end
      object CGIMaxInactivityLabel: TLabel
        Left = 8
        Top = 132
        Width = 90
        Height = 21
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object CGIContentLabel: TLabel
        Left = 8
        Top = 102
        Width = 90
        Height = 23
        AutoSize = False
        Caption = 'Content-Type'
      end
      object CGIDirEdit: TEdit
        Left = 98
        Top = 42
        Width = 180
        Height = 23
        Hint = 'CGI sub-directory (off of HTML root)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object CGICheckBox: TCheckBox
        Left = 8
        Top = 12
        Width = 263
        Height = 23
        Hint = 'CGI support is enabled when checked'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = CGICheckBoxClick
      end
      object CGIExtEdit: TEdit
        Left = 98
        Top = 72
        Width = 180
        Height = 23
        Hint = 'File extensions that denote CGI executable files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object CGIMaxInactivityEdit: TEdit
        Left = 98
        Top = 132
        Width = 45
        Height = 23
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=120)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object CGIContentEdit: TEdit
        Left = 98
        Top = 102
        Width = 180
        Height = 23
        Hint = 'Default Content-Type for CGI output'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object CGIEnvButton: TButton
        Left = 8
        Top = 165
        Width = 135
        Height = 23
        Caption = 'Environment Vars'
        TabOrder = 5
        OnClick = CGIEnvButtonClick
      end
      object WebHandlersButton: TButton
        Left = 150
        Top = 165
        Width = 128
        Height = 23
        Caption = 'Content Handlers'
        TabOrder = 6
        OnClick = WebHandlersButtonClick
      end
    end
    object LogTabSheet: TTabSheet
      Caption = 'Log'
      ImageIndex = 1
      object LogBaseLabel: TLabel
        Left = 8
        Top = 102
        Width = 90
        Height = 23
        AutoSize = False
        Caption = 'Base Filename'
      end
      object DebugTxCheckBox: TCheckBox
        Left = 8
        Top = 39
        Width = 180
        Height = 23
        Hint = 'Log (debug) transmitted HTTP responses'
        Caption = 'Transmitted Responses'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DebugRxCheckBox: TCheckBox
        Left = 8
        Top = 12
        Width = 180
        Height = 21
        Hint = 'Log (debug) all received HTTP requests'
        Caption = 'Received Requests'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object AccessLogCheckBox: TCheckBox
        Left = 8
        Top = 68
        Width = 180
        Height = 22
        Hint = 'Create HTTP access log files'
        Caption = 'Create Access Log Files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        OnClick = AccessLogCheckBoxClick
      end
      object LogBaseNameEdit: TEdit
        Left = 98
        Top = 102
        Width = 180
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
    Top = 243
    Width = 88
    Height = 29
    Anchors = [akLeft, akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 120
    Top = 243
    Width = 87
    Height = 29
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object ApplyBtn: TButton
    Left = 218
    Top = 243
    Width = 88
    Height = 29
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
