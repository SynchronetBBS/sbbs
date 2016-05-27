object PropertiesDlg: TPropertiesDlg
  Left = 688
  Top = 261
  BorderStyle = bsDialog
  Caption = 'Control Panel Properties'
  ClientHeight = 234
  ClientWidth = 352
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  ShowHint = True
  OnShow = FormShow
  DesignSize = (
    352
    234)
  PixelsPerInch = 96
  TextHeight = 13
  object OKBtn: TButton
    Left = 267
    Top = 8
    Width = 76
    Height = 25
    Anchors = [akTop, akRight]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
  end
  object CancelBtn: TButton
    Left = 267
    Top = 38
    Width = 76
    Height = 25
    Anchors = [akTop, akRight]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object PageControl: TPageControl
    Left = 7
    Top = 7
    Width = 254
    Height = 221
    ActivePage = SettingsTabSheet
    Anchors = [akLeft, akTop, akBottom]
    TabIndex = 0
    TabOrder = 2
    object SettingsTabSheet: TTabSheet
      Caption = 'Settings'
      object Label3: TLabel
        Left = 7
        Top = 10
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Login Command'
      end
      object Label2: TLabel
        Left = 7
        Top = 36
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Config Command'
      end
      object Label4: TLabel
        Left = 7
        Top = 62
        Width = 182
        Height = 19
        AutoSize = False
        Caption = 'Node Display Interval (seconds)'
      end
      object Label5: TLabel
        Left = 7
        Top = 88
        Width = 182
        Height = 19
        AutoSize = False
        Caption = 'Client Display Interval (seconds)'
      end
      object PasswordLabel: TLabel
        Left = 7
        Top = 167
        Width = 89
        Height = 20
        AutoSize = False
        Caption = 'Password'
      end
      object Label10: TLabel
        Left = 7
        Top = 112
        Width = 182
        Height = 19
        AutoSize = False
        Caption = 'Semaphore Check Interval (seconds)'
      end
      object LoginCmdEdit: TEdit
        Left = 98
        Top = 10
        Width = 137
        Height = 21
        Hint = 'Login command-line or URL (default = telnet://127.0.0.1)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object ConfigCmdEdit: TEdit
        Left = 98
        Top = 36
        Width = 137
        Height = 21
        Hint = 'Configuration command line'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object NodeIntEdit: TEdit
        Left = 195
        Top = 62
        Width = 20
        Height = 21
        Hint = 'Frequency of updates to Node window'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        Text = '1'
      end
      object NodeIntUpDown: TUpDown
        Left = 215
        Top = 62
        Width = 16
        Height = 21
        Associate = NodeIntEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 3
        Wrap = False
      end
      object ClientIntEdit: TEdit
        Left = 195
        Top = 88
        Width = 20
        Height = 21
        Hint = 'Frequency of updates to clients window'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
        Text = '1'
      end
      object ClientIntUpDown: TUpDown
        Left = 215
        Top = 88
        Width = 16
        Height = 21
        Associate = ClientIntEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 5
        Wrap = False
      end
      object TrayIconCheckBox: TCheckBox
        Left = 7
        Top = 138
        Width = 228
        Height = 20
        Hint = 'Create tray icon when minimized'
        Caption = 'Minimize to System Tray'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
        OnClick = TrayIconCheckBoxClick
      end
      object PasswordEdit: TEdit
        Left = 98
        Top = 167
        Width = 137
        Height = 21
        Hint = 'Required password for restoring from system tray icon'
        ParentShowHint = False
        PasswordChar = '*'
        ShowHint = True
        TabOrder = 9
      end
      object SemFreqEdit: TEdit
        Left = 195
        Top = 112
        Width = 20
        Height = 21
        Hint = 'Frequency of checks for signaled semaphore files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
        Text = '1'
      end
      object SemFreqUpDown: TUpDown
        Left = 215
        Top = 112
        Width = 15
        Height = 21
        Associate = SemFreqEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 7
        Wrap = False
      end
    end
    object SecurityTabSheet: TTabSheet
      Caption = 'Security'
      ImageIndex = 4
      object FailedLoginAttemptGroupBox: TGroupBox
        Left = 8
        Top = 8
        Width = 225
        Height = 177
        Caption = 'Failed Login Attempts'
        TabOrder = 0
        object LoginAttemptDelayLabel: TLabel
          Left = 8
          Top = 24
          Width = 92
          Height = 13
          Caption = 'Delay (milliseconds)'
        end
        object LoginAttemptThrottleLabel: TLabel
          Left = 8
          Top = 48
          Width = 101
          Height = 13
          Caption = 'Throttle (milliseconds)'
        end
        object LoginAttemptHackThreshold: TLabel
          Left = 8
          Top = 72
          Width = 97
          Height = 13
          Caption = 'Hack Log Threshold'
        end
        object LoginAttemptTempBanThresholdLabel: TLabel
          Left = 8
          Top = 96
          Width = 99
          Height = 13
          Caption = 'Temp Ban Threshold'
        end
        object LoginAttemptTempBanDurationLabel: TLabel
          Left = 8
          Top = 120
          Width = 92
          Height = 13
          Caption = 'Temp Ban Duration'
        end
        object LoginAttemptFilterThresholdLabel: TLabel
          Left = 8
          Top = 144
          Width = 99
          Height = 13
          Caption = 'Perm Filter Threshold'
        end
        object LoginAttemptDelayEdit: TEdit
          Left = 128
          Top = 24
          Width = 65
          Height = 21
          Hint = 'Delay after each failed login attempt'
          TabOrder = 0
        end
        object LoginAttemptThrottleEdit: TEdit
          Left = 128
          Top = 48
          Width = 65
          Height = 21
          Hint = 'Delay successive connections after a failed login attempt'
          TabOrder = 1
        end
        object LoginAttemptHackThresholdEdit: TEdit
          Left = 128
          Top = 72
          Width = 65
          Height = 21
          Hint = 'Consecutive failed login attempts before logging hack attempt'
          TabOrder = 2
        end
        object LoginAttemptTempBanThresholdEdit: TEdit
          Left = 128
          Top = 96
          Width = 65
          Height = 21
          Hint = 
            'Consecutive failed login attempts before temporary ban of client' +
            ' IP'
          TabOrder = 3
        end
        object LoginAttemptTempBanDurationEdit: TEdit
          Left = 128
          Top = 120
          Width = 65
          Height = 21
          Hint = 'Duration of temporary ban of client IP'
          TabOrder = 4
        end
        object LoginAttemptFilterThresholdEdit: TEdit
          Left = 128
          Top = 144
          Width = 65
          Height = 21
          Hint = 
            'Consecutive failed login attempts before client IP is filtered (' +
            'permanently blocked )'
          TabOrder = 5
        end
      end
    end
    object CustomizeTabSheet: TTabSheet
      Caption = 'Customize'
      ImageIndex = 1
      object SourceComboBox: TComboBox
        Left = 7
        Top = 10
        Width = 109
        Height = 21
        ItemHeight = 13
        TabOrder = 0
        Text = 'Node List'
        OnChange = SourceComboBoxChange
        Items.Strings = (
          'Node List'
          'Client List'
          'Terminal Server Log'
          'Event Log'
          'FTP Server Log'
          'Mail Server Log'
          'Web Server Log'
          'Services Log')
      end
      object ExampleEdit: TEdit
        Left = 125
        Top = 10
        Width = 110
        Height = 21
        TabOrder = 1
        Text = 'Scheme'
      end
      object FontButton: TButton
        Left = 7
        Top = 36
        Width = 109
        Height = 19
        Caption = 'Change Font'
        TabOrder = 2
        OnClick = FontButtonClick
      end
      object BackgroundButton: TButton
        Left = 125
        Top = 36
        Width = 110
        Height = 19
        Caption = 'Background Color'
        TabOrder = 3
        OnClick = BackgroundButtonClick
      end
      object ApplyButton: TButton
        Left = 7
        Top = 62
        Width = 109
        Height = 19
        Caption = 'Apply Scheme To:'
        TabOrder = 4
        OnClick = ApplyButtonClick
      end
      object TargetComboBox: TComboBox
        Left = 125
        Top = 62
        Width = 110
        Height = 21
        ItemHeight = 13
        TabOrder = 5
        Items.Strings = (
          'Node List'
          'Client List'
          'Terminal Server Log'
          'Event Log'
          'FTP Server Log'
          'Mail Server Log'
          'Web Server Log'
          'Services Log'
          'All  Windows')
      end
      object LogFontGroupBox: TGroupBox
        Left = 7
        Top = 104
        Width = 228
        Height = 79
        Caption = 'Log Fonts'
        TabOrder = 6
        object LogLevelLabel: TLabel
          Left = 18
          Top = 21
          Width = 92
          Height = 19
          Alignment = taRightJustify
          AutoSize = False
          Caption = 'Log Level'
        end
        object LogLevelComboBox: TComboBox
          Left = 119
          Top = 20
          Width = 96
          Height = 21
          ItemHeight = 13
          ItemIndex = 7
          TabOrder = 0
          Text = 'Debug'
          OnChange = LogLevelComboBoxChange
          Items.Strings = (
            'Emergency'
            'Alert'
            'Critical'
            'Error'
            'Warning'
            'Notice'
            'Normal'
            'Debug')
        end
        object LogFontExampleEdit: TEdit
          Left = 119
          Top = 46
          Width = 96
          Height = 21
          TabOrder = 1
          Text = 'Example'
        end
        object LogFontButton: TButton
          Left = 13
          Top = 46
          Width = 99
          Height = 19
          Caption = 'Change Font'
          TabOrder = 2
          OnClick = LogFontButtonClick
        end
      end
    end
    object AdvancedTabSheet: TTabSheet
      Caption = 'Advanced'
      ImageIndex = 2
      object Label1: TLabel
        Left = 7
        Top = 10
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Control Directory'
      end
      object Label6: TLabel
        Left = 7
        Top = 62
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Hostname'
      end
      object Label8: TLabel
        Left = 7
        Top = 88
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Log Window Size'
      end
      object Label9: TLabel
        Left = 7
        Top = 36
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Temp Directory'
      end
      object ErrorSoundLabel: TLabel
        Left = 7
        Top = 114
        Width = 65
        Height = 20
        AutoSize = False
        Caption = 'Error Sound'
      end
      object CtrlDirEdit: TEdit
        Left = 98
        Top = 10
        Width = 137
        Height = 21
        Hint = 'Control directory (e.g. c:\sbbs\ctrl)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object HostnameEdit: TEdit
        Left = 98
        Top = 62
        Width = 137
        Height = 21
        Hint = 'Hostname (if different than configured in SCFG)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object MaxLogLenEdit: TEdit
        Left = 98
        Top = 88
        Width = 137
        Height = 21
        Hint = 
          'Maximum number of lines to store in log windows before auto-dele' +
          'ting old lines'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object TempDirEdit: TEdit
        Left = 98
        Top = 36
        Width = 137
        Height = 21
        Hint = 'Temp directory (e.g. C:\SBBSTEMP)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object UndockableCheckBox: TCheckBox
        Left = 7
        Top = 144
        Width = 228
        Height = 20
        Hint = 'Allow child windows to be "un-docked" from main window'
        Caption = 'Undockable Windows'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object FileAssociationsCheckBox: TCheckBox
        Left = 7
        Top = 170
        Width = 228
        Height = 20
        Hint = 'Use Windows file associations when viewing or editing files'
        Caption = 'Use File Associations'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object ErrorSoundEdit: TEdit
        Left = 98
        Top = 114
        Width = 112
        Height = 21
        Hint = 'Sound file to play when an error condition is logged'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object ErrorSoundButton: TButton
        Left = 214
        Top = 114
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 5
        OnClick = ErrorSoundButtonClick
      end
    end
    object JavaScriptTabSheet: TTabSheet
      Caption = 'JavaScript'
      ImageIndex = 3
      object Label7: TLabel
        Left = 7
        Top = 10
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Heap Size'
      end
      object Label11: TLabel
        Left = 7
        Top = 36
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Context Stack'
      end
      object Label12: TLabel
        Left = 7
        Top = 62
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Time Limit'
      end
      object Label13: TLabel
        Left = 7
        Top = 88
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'GC Interval'
      end
      object Label14: TLabel
        Left = 7
        Top = 114
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Yield Interval'
      end
      object Label16: TLabel
        Left = 7
        Top = 140
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'Load Path'
      end
      object JS_MaxBytesEdit: TEdit
        Left = 98
        Top = 10
        Width = 137
        Height = 21
        Hint = 
          'Maximum number of bytes that can be allocated per runtime before' +
          ' garbage collection'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object JS_ContextStackEdit: TEdit
        Left = 98
        Top = 36
        Width = 137
        Height = 21
        Hint = 'Size of context stack (in bytes)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object JS_TimeLimitEdit: TEdit
        Left = 98
        Top = 62
        Width = 137
        Height = 21
        Hint = 
          'Maximum number of 100ms ticks before triggering "infinite loop" ' +
          'detection (0=disabled)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object JS_GcIntervalEdit: TEdit
        Left = 98
        Top = 88
        Width = 137
        Height = 21
        Hint = 
          'Number of ticks between attempted garbage collections (0=disable' +
          'd)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object JS_YieldIntervalEdit: TEdit
        Left = 98
        Top = 114
        Width = 137
        Height = 21
        Hint = 'Number of ticks between forced yields (0=disabled)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object JS_LoadPathEdit: TEdit
        Left = 98
        Top = 140
        Width = 137
        Height = 21
        Hint = 'Comma-separated list of directories to search for loaded scripts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
    end
  end
  object HelpBtn: TButton
    Left = 267
    Top = 70
    Width = 76
    Height = 25
    Anchors = [akTop, akRight]
    Cancel = True
    Caption = 'Help'
    TabOrder = 3
    OnClick = HelpBtnClick
  end
  object FontDialog1: TFontDialog
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    MinFontSize = 0
    MaxFontSize = 0
    Left = 376
    Top = 184
  end
  object ColorDialog1: TColorDialog
    Ctl3D = True
    Left = 368
    Top = 232
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Options = [ofHideReadOnly, ofNoChangeDir, ofEnableSizing, ofDontAddToRecent]
    Left = 376
    Top = 152
  end
end
