object PropertiesDlg: TPropertiesDlg
  Left = 631
  Top = 219
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
        Hint = 'Login command-line or URL (default = telnet://localhost)'
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
        TabOrder = 6
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
        TabOrder = 7
      end
      object SemFreqEdit: TEdit
        Left = 195
        Top = 112
        Width = 20
        Height = 21
        Hint = 'Frequency of checks for signaled semaphore files'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
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
        TabOrder = 9
        Wrap = False
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
      object Label7: TLabel
        Left = 7
        Top = 88
        Width = 89
        Height = 19
        AutoSize = False
        Caption = 'JavaScript Heap'
      end
      object Label8: TLabel
        Left = 7
        Top = 114
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
        TabOrder = 1
      end
      object JS_MaxBytesEdit: TEdit
        Left = 98
        Top = 88
        Width = 137
        Height = 21
        Hint = 
          'Maximum number of bytes that can be allocated for use by the Jav' +
          'aScript engine'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object MaxLogLenEdit: TEdit
        Left = 98
        Top = 114
        Width = 137
        Height = 21
        Hint = 
          'Maximum number of bytes to store in log windows before auto-dele' +
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
        Hint = 'Temp directory (e.g. C:\TEMP)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
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
        TabOrder = 5
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
