object PropertiesDlg: TPropertiesDlg
  Left = 631
  Top = 219
  BorderStyle = bsDialog
  Caption = 'Control Panel Properties'
  ClientHeight = 288
  ClientWidth = 433
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  ShowHint = True
  OnShow = FormShow
  DesignSize = (
    433
    288)
  PixelsPerInch = 120
  TextHeight = 16
  object OKBtn: TButton
    Left = 329
    Top = 10
    Width = 93
    Height = 31
    Anchors = [akTop, akRight]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
  end
  object CancelBtn: TButton
    Left = 329
    Top = 47
    Width = 93
    Height = 31
    Anchors = [akTop, akRight]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object PageControl: TPageControl
    Left = 9
    Top = 9
    Width = 312
    Height = 272
    ActivePage = SettingsTabSheet
    Anchors = [akLeft, akTop, akBottom]
    TabIndex = 0
    TabOrder = 2
    object SettingsTabSheet: TTabSheet
      Caption = 'Settings'
      object Label3: TLabel
        Left = 9
        Top = 12
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Login Command'
      end
      object Label2: TLabel
        Left = 9
        Top = 44
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Config Command'
      end
      object Label4: TLabel
        Left = 9
        Top = 76
        Width = 224
        Height = 24
        AutoSize = False
        Caption = 'Node Display Interval (seconds)'
      end
      object Label5: TLabel
        Left = 9
        Top = 108
        Width = 224
        Height = 24
        AutoSize = False
        Caption = 'Client Display Interval (seconds)'
      end
      object PasswordLabel: TLabel
        Left = 9
        Top = 206
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Password'
      end
      object Label10: TLabel
        Left = 9
        Top = 138
        Width = 224
        Height = 23
        AutoSize = False
        Caption = 'Semaphore Check Interval (seconds)'
      end
      object LoginCmdEdit: TEdit
        Left = 121
        Top = 12
        Width = 168
        Height = 24
        Hint = 'Login command-line or URL (default = telnet://localhost)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object ConfigCmdEdit: TEdit
        Left = 121
        Top = 44
        Width = 168
        Height = 24
        Hint = 'Configuration command line'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object NodeIntEdit: TEdit
        Left = 240
        Top = 76
        Width = 25
        Height = 24
        Hint = 'Frequency of updates to Node window'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        Text = '1'
      end
      object NodeIntUpDown: TUpDown
        Left = 265
        Top = 76
        Width = 19
        Height = 30
        Associate = NodeIntEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 3
        Wrap = False
      end
      object ClientIntEdit: TEdit
        Left = 240
        Top = 108
        Width = 25
        Height = 24
        Hint = 'Frequency of updates to clients window'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
        Text = '1'
      end
      object ClientIntUpDown: TUpDown
        Left = 265
        Top = 108
        Width = 19
        Height = 30
        Associate = ClientIntEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 5
        Wrap = False
      end
      object TrayIconCheckBox: TCheckBox
        Left = 9
        Top = 170
        Width = 280
        Height = 24
        Hint = 'Create tray icon when minimized'
        Caption = 'Minimize to System Tray'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
        OnClick = TrayIconCheckBoxClick
      end
      object PasswordEdit: TEdit
        Left = 121
        Top = 206
        Width = 168
        Height = 24
        Hint = 'Required password for restoring from system tray icon'
        ParentShowHint = False
        PasswordChar = '*'
        ShowHint = True
        TabOrder = 7
      end
      object SemFreqEdit: TEdit
        Left = 240
        Top = 138
        Width = 25
        Height = 24
        Hint = 'Frequency of checks for signaled semaphore files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
        Text = '1'
      end
      object SemFreqUpDown: TUpDown
        Left = 265
        Top = 138
        Width = 18
        Height = 29
        Associate = SemFreqEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 9
        Wrap = False
      end
    end
    object CustomizeTabSheet: TTabSheet
      Caption = 'Customize'
      ImageIndex = 1
      object SourceComboBox: TComboBox
        Left = 9
        Top = 12
        Width = 134
        Height = 24
        ItemHeight = 16
        ItemIndex = 0
        TabOrder = 0
        Text = 'Node List'
        OnChange = SourceComboBoxChange
        Items.Strings = (
          'Node List'
          'Client List'
          'Telnet Server Log'
          'Event Log'
          'FTP Server Log'
          'Mail Server Log'
          'Services Log')
      end
      object ExampleEdit: TEdit
        Left = 154
        Top = 12
        Width = 135
        Height = 24
        TabOrder = 1
        Text = 'Scheme'
      end
      object FontButton: TButton
        Left = 9
        Top = 44
        Width = 134
        Height = 24
        Caption = 'Change Font'
        TabOrder = 2
        OnClick = FontButtonClick
      end
      object BackgroundButton: TButton
        Left = 154
        Top = 44
        Width = 135
        Height = 24
        Caption = 'Background Color'
        TabOrder = 3
        OnClick = BackgroundButtonClick
      end
      object ApplyButton: TButton
        Left = 9
        Top = 76
        Width = 134
        Height = 24
        Caption = 'Apply Scheme To:'
        TabOrder = 4
        OnClick = ApplyButtonClick
      end
      object TargetComboBox: TComboBox
        Left = 154
        Top = 76
        Width = 135
        Height = 24
        ItemHeight = 16
        TabOrder = 5
        Items.Strings = (
          'Node List'
          'Client List'
          'Telnet Server Log'
          'Event Log'
          'FTP Server Log'
          'Mail Server Log'
          'Services Log'
          'All  Windows')
      end
    end
    object AdvancedTabSheet: TTabSheet
      Caption = 'Advanced'
      ImageIndex = 2
      object Label1: TLabel
        Left = 9
        Top = 12
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Control Directory'
      end
      object Label6: TLabel
        Left = 9
        Top = 76
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Hostname'
      end
      object Label8: TLabel
        Left = 9
        Top = 108
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Log Window Size'
      end
      object Label9: TLabel
        Left = 9
        Top = 44
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Temp Directory'
      end
      object CtrlDirEdit: TEdit
        Left = 121
        Top = 12
        Width = 168
        Height = 24
        Hint = 'Control directory (e.g. c:\sbbs\ctrl)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object HostnameEdit: TEdit
        Left = 121
        Top = 76
        Width = 168
        Height = 24
        Hint = 'Hostname (if different than configured in SCFG)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object MaxLogLenEdit: TEdit
        Left = 121
        Top = 108
        Width = 168
        Height = 24
        Hint = 
          'Maximum number of bytes to store in log windows before auto-dele' +
          'ting old lines'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object TempDirEdit: TEdit
        Left = 121
        Top = 44
        Width = 168
        Height = 24
        Hint = 'Temp directory (e.g. C:\TEMP)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object UndockableCheckBox: TCheckBox
        Left = 9
        Top = 145
        Width = 280
        Height = 25
        Hint = 'Allow child windows to be "un-docked" from main window'
        Caption = 'Undockable Windows'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
    end
    object JavaScriptTabSheet: TTabSheet
      Caption = 'JavaScript'
      ImageIndex = 3
      object Label7: TLabel
        Left = 9
        Top = 12
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Heap Size'
      end
      object Label11: TLabel
        Left = 9
        Top = 44
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Stack Size'
      end
      object Label12: TLabel
        Left = 9
        Top = 76
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Branch Limit'
      end
      object Label13: TLabel
        Left = 9
        Top = 108
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'GC Interval'
      end
      object Label14: TLabel
        Left = 9
        Top = 140
        Width = 109
        Height = 24
        AutoSize = False
        Caption = 'Yield Interval'
      end
      object JS_MaxBytesEdit: TEdit
        Left = 121
        Top = 12
        Width = 168
        Height = 24
        Hint = 'Maximum number of bytes that can be allocated per runtime'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object JS_ContextStackEdit: TEdit
        Left = 121
        Top = 44
        Width = 168
        Height = 24
        Hint = 'Size of context stack (in bytes)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object JS_BranchLimitEdit: TEdit
        Left = 121
        Top = 76
        Width = 168
        Height = 24
        Hint = 
          'Maximum number of branches before triggering "infinite loop" det' +
          'ection (0=disabled)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object JS_GcIntervalEdit: TEdit
        Left = 121
        Top = 108
        Width = 168
        Height = 24
        Hint = 
          'Number of branches between attempted garbage collections (0=disa' +
          'bled)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object JS_YieldIntervalEdit: TEdit
        Left = 121
        Top = 140
        Width = 168
        Height = 24
        Hint = 'Number of branches between forced yields (0=disabled)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
    end
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
end
