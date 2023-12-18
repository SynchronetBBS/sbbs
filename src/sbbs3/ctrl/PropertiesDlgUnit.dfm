object PropertiesDlg: TPropertiesDlg
  Left = 1041
  Top = 844
  BorderStyle = bsDialog
  Caption = 'Control Panel Properties'
  ClientHeight = 270
  ClientWidth = 406
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Microsoft Sans Serif'
  Font.Style = []
  OldCreateOrder = True
  Position = poScreenCenter
  ShowHint = True
  OnShow = FormShow
  DesignSize = (
    406
    270)
  PixelsPerInch = 96
  TextHeight = 15
  object OKBtn: TButton
    Left = 308
    Top = 9
    Width = 88
    Height = 29
    Anchors = [akTop, akRight]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
  end
  object CancelBtn: TButton
    Left = 308
    Top = 44
    Width = 88
    Height = 29
    Anchors = [akTop, akRight]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object PageControl: TPageControl
    Left = 8
    Top = 8
    Width = 293
    Height = 255
    ActivePage = SecurityTabSheet
    Anchors = [akLeft, akTop, akBottom]
    TabIndex = 1
    TabOrder = 2
    object SettingsTabSheet: TTabSheet
      Caption = 'Settings'
      object Label3: TLabel
        Left = 8
        Top = 12
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Login Command'
      end
      object Label2: TLabel
        Left = 8
        Top = 42
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Config Command'
      end
      object Label4: TLabel
        Left = 8
        Top = 72
        Width = 210
        Height = 21
        AutoSize = False
        Caption = 'Node Display Interval (seconds)'
      end
      object Label5: TLabel
        Left = 8
        Top = 102
        Width = 210
        Height = 21
        AutoSize = False
        Caption = 'Client Display Interval (seconds)'
      end
      object PasswordLabel: TLabel
        Left = 8
        Top = 193
        Width = 103
        Height = 23
        AutoSize = False
        Caption = 'Password'
      end
      object Label10: TLabel
        Left = 8
        Top = 129
        Width = 210
        Height = 22
        AutoSize = False
        Caption = 'Semaphore Check Interval (seconds)'
      end
      object LoginCmdEdit: TEdit
        Left = 113
        Top = 12
        Width = 158
        Height = 23
        Hint = 'Login command-line or URL (default = telnet://127.0.0.1)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object ConfigCmdEdit: TEdit
        Left = 113
        Top = 42
        Width = 158
        Height = 23
        Hint = 'Configuration command line'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object NodeIntEdit: TEdit
        Left = 225
        Top = 72
        Width = 23
        Height = 23
        Hint = 'Frequency of updates to Node window'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        Text = '1'
      end
      object NodeIntUpDown: TUpDown
        Left = 248
        Top = 72
        Width = 19
        Height = 23
        Associate = NodeIntEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 3
        Wrap = False
      end
      object ClientIntEdit: TEdit
        Left = 225
        Top = 102
        Width = 23
        Height = 23
        Hint = 'Frequency of updates to clients window'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
        Text = '1'
      end
      object ClientIntUpDown: TUpDown
        Left = 248
        Top = 102
        Width = 19
        Height = 23
        Associate = ClientIntEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 5
        Wrap = False
      end
      object TrayIconCheckBox: TCheckBox
        Left = 8
        Top = 159
        Width = 263
        Height = 23
        Hint = 'Create tray icon when minimized'
        Caption = 'Minimize to System Tray'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
        OnClick = TrayIconCheckBoxClick
      end
      object PasswordEdit: TEdit
        Left = 113
        Top = 193
        Width = 158
        Height = 23
        Hint = 'Required password for restoring from system tray icon'
        ParentShowHint = False
        PasswordChar = '*'
        ShowHint = True
        TabOrder = 9
      end
      object SemFreqEdit: TEdit
        Left = 225
        Top = 129
        Width = 23
        Height = 23
        Hint = 'Frequency of checks for signaled semaphore files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
        Text = '1'
      end
      object SemFreqUpDown: TUpDown
        Left = 248
        Top = 129
        Width = 17
        Height = 23
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
        Left = 9
        Top = 1
        Width = 260
        Height = 221
        Caption = 'Failed Login Attempts'
        TabOrder = 0
        object LoginAttemptDelayLabel: TLabel
          Left = 9
          Top = 24
          Width = 111
          Height = 15
          Caption = 'Delay (milliseconds)'
        end
        object LoginAttemptThrottleLabel: TLabel
          Left = 9
          Top = 51
          Width = 121
          Height = 15
          Caption = 'Throttle (milliseconds)'
        end
        object LoginAttemptHackThreshold: TLabel
          Left = 9
          Top = 79
          Width = 110
          Height = 15
          Caption = 'Hack Log Threshold'
        end
        object LoginAttemptTempBanThresholdLabel: TLabel
          Left = 9
          Top = 107
          Width = 115
          Height = 15
          Caption = 'Temp Ban Threshold'
        end
        object LoginAttemptTempBanDurationLabel: TLabel
          Left = 9
          Top = 134
          Width = 107
          Height = 15
          Caption = 'Temp Ban Duration'
        end
        object LoginAttemptFilterThresholdLabel: TLabel
          Left = 9
          Top = 162
          Width = 112
          Height = 15
          Caption = 'Auto Filter Threshold'
        end
        object LoginAttemptFilterDurationLabel: TLabel
          Left = 9
          Top = 190
          Width = 104
          Height = 15
          Caption = 'Auto Filter Duration'
        end
        object LoginAttemptDelayEdit: TEdit
          Left = 148
          Top = 24
          Width = 75
          Height = 23
          Hint = 'Delay after each failed login attempt'
          TabOrder = 0
        end
        object LoginAttemptThrottleEdit: TEdit
          Left = 148
          Top = 51
          Width = 75
          Height = 23
          Hint = 'Delay successive connections after a failed login attempt'
          TabOrder = 1
        end
        object LoginAttemptHackThresholdEdit: TEdit
          Left = 148
          Top = 79
          Width = 75
          Height = 23
          Hint = 'Consecutive failed login attempts before logging hack attempt'
          TabOrder = 2
        end
        object LoginAttemptTempBanThresholdEdit: TEdit
          Left = 148
          Top = 107
          Width = 75
          Height = 23
          Hint = 
            'Consecutive failed login attempts before temporary ban of client' +
            ' IP'
          TabOrder = 3
        end
        object LoginAttemptTempBanDurationEdit: TEdit
          Left = 148
          Top = 134
          Width = 75
          Height = 23
          Hint = 'Duration of temporary ban of client IP'
          TabOrder = 4
        end
        object LoginAttemptFilterThresholdEdit: TEdit
          Left = 148
          Top = 162
          Width = 75
          Height = 23
          Hint = 
            'Consecutive failed login attempts before client IP is persistent' +
            'ly filtered'
          TabOrder = 5
        end
        object LoginAttemptFilterDurationEdit: TEdit
          Left = 148
          Top = 190
          Width = 75
          Height = 23
          Hint = 'Duration of persistent client IP filters'
          TabOrder = 6
        end
      end
    end
    object CustomizeTabSheet: TTabSheet
      Caption = 'Customize'
      ImageIndex = 1
      object SourceComboBox: TComboBox
        Left = 8
        Top = 12
        Width = 126
        Height = 23
        ItemHeight = 15
        TabOrder = 0
        Text = 'Source'
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
        Left = 144
        Top = 12
        Width = 127
        Height = 21
        TabOrder = 1
        Text = 'Scheme'
      end
      object FontButton: TButton
        Left = 8
        Top = 42
        Width = 126
        Height = 21
        Caption = 'Change Font'
        TabOrder = 2
        OnClick = FontButtonClick
      end
      object BackgroundButton: TButton
        Left = 144
        Top = 42
        Width = 127
        Height = 21
        Caption = 'Background Color'
        TabOrder = 3
        OnClick = BackgroundButtonClick
      end
      object ApplyButton: TButton
        Left = 8
        Top = 72
        Width = 126
        Height = 21
        Caption = 'Apply Scheme To:'
        TabOrder = 4
        OnClick = ApplyButtonClick
      end
      object TargetComboBox: TComboBox
        Left = 144
        Top = 72
        Width = 127
        Height = 23
        ItemHeight = 15
        TabOrder = 5
        Text = 'Target'
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
        Left = 8
        Top = 120
        Width = 263
        Height = 91
        Caption = 'Log Fonts'
        TabOrder = 6
        object LogLevelLabel: TLabel
          Left = 21
          Top = 24
          Width = 106
          Height = 22
          Alignment = taRightJustify
          AutoSize = False
          Caption = 'Log Level'
        end
        object LogLevelComboBox: TComboBox
          Left = 137
          Top = 23
          Width = 111
          Height = 23
          ItemHeight = 15
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
          Left = 137
          Top = 53
          Width = 111
          Height = 21
          TabOrder = 1
          Text = 'Example'
        end
        object LogFontButton: TButton
          Left = 15
          Top = 53
          Width = 114
          Height = 22
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
        Left = 8
        Top = 12
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Control Directory'
      end
      object Label6: TLabel
        Left = 8
        Top = 72
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Hostname'
      end
      object Label8: TLabel
        Left = 8
        Top = 102
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Log Window Size'
      end
      object Label9: TLabel
        Left = 8
        Top = 42
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Temp Directory'
      end
      object CtrlDirEdit: TEdit
        Left = 113
        Top = 12
        Width = 158
        Height = 21
        Hint = 'Control directory (e.g. c:\sbbs\ctrl)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object HostnameEdit: TEdit
        Left = 113
        Top = 72
        Width = 158
        Height = 21
        Hint = 'Hostname (if different than configured in SCFG)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object MaxLogLenEdit: TEdit
        Left = 113
        Top = 102
        Width = 158
        Height = 21
        Hint = 
          'Maximum number of lines to store in log windows before auto-dele' +
          'ting old lines'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object TempDirEdit: TEdit
        Left = 113
        Top = 42
        Width = 158
        Height = 21
        Hint = 'Temp directory (e.g. C:\SBBSTEMP)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object UndockableCheckBox: TCheckBox
        Left = 8
        Top = 166
        Width = 263
        Height = 23
        Hint = 'Allow child windows to be "un-docked" from main window'
        Caption = 'Undockable Windows'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object FileAssociationsCheckBox: TCheckBox
        Left = 8
        Top = 196
        Width = 263
        Height = 23
        Hint = 'Use Windows file associations when viewing or editing files'
        Caption = 'Use File Associations'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
    end
    object JavaScriptTabSheet: TTabSheet
      Caption = 'JavaScript'
      ImageIndex = 3
      object Label7: TLabel
        Left = 8
        Top = 12
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Heap Size'
      end
      object Label11: TLabel
        Left = 8
        Top = 42
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Context Stack'
        Enabled = False
      end
      object Label12: TLabel
        Left = 8
        Top = 72
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Time Limit'
      end
      object Label13: TLabel
        Left = 8
        Top = 102
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'GC Interval'
      end
      object Label14: TLabel
        Left = 8
        Top = 132
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Yield Interval'
      end
      object Label16: TLabel
        Left = 8
        Top = 162
        Width = 103
        Height = 21
        AutoSize = False
        Caption = 'Load Path'
      end
      object JS_MaxBytesEdit: TEdit
        Left = 113
        Top = 12
        Width = 158
        Height = 21
        Hint = 
          'Maximum number of bytes that can be allocated per runtime before' +
          ' garbage collection'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object JS_ContextStackEdit: TEdit
        Left = 113
        Top = 42
        Width = 158
        Height = 21
        Hint = 'Size of context stack (in bytes)'
        Enabled = False
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object JS_TimeLimitEdit: TEdit
        Left = 113
        Top = 72
        Width = 158
        Height = 21
        Hint = 
          'Maximum number of 100ms ticks before triggering "infinite loop" ' +
          'detection (0=disabled)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object JS_GcIntervalEdit: TEdit
        Left = 113
        Top = 102
        Width = 158
        Height = 21
        Hint = 
          'Number of ticks between attempted garbage collections (0=disable' +
          'd)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object JS_YieldIntervalEdit: TEdit
        Left = 113
        Top = 132
        Width = 158
        Height = 21
        Hint = 'Number of ticks between forced yields (0=disabled)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object JS_LoadPathEdit: TEdit
        Left = 113
        Top = 162
        Width = 158
        Height = 21
        Hint = 'Comma-separated list of directories to search for loaded scripts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
    end
    object Sound: TTabSheet
      Caption = 'Sound'
      ImageIndex = 5
      object ErrorSoundLabel: TLabel
        Left = 8
        Top = 12
        Width = 75
        Height = 23
        AutoSize = False
        Caption = 'Error Sound'
      end
      object ConfigureSoundButton: TButton
        Left = 9
        Top = 46
        Width = 260
        Height = 29
        Caption = 'Configure Common Server Event Sounds'
        TabOrder = 0
        OnClick = ConfigureSoundButtonClick
      end
      object ErrorSoundEdit: TEdit
        Left = 83
        Top = 12
        Width = 159
        Height = 23
        Hint = 'Sound file to play when an error condition is logged'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object ErrorSoundButton: TButton
        Left = 247
        Top = 12
        Width = 23
        Height = 24
        Caption = '...'
        TabOrder = 2
        OnClick = ErrorSoundButtonClick
      end
    end
  end
  object HelpBtn: TButton
    Left = 308
    Top = 81
    Width = 88
    Height = 29
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
    Font.Name = 'Microsoft Sans Serif'
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
