object PropertiesDlg: TPropertiesDlg
  Left = 624
  Top = 358
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
    Left = 8
    Top = 8
    Width = 313
    Height = 273
    ActivePage = AdvancedTabSheet
    Anchors = [akLeft, akTop, akBottom]
    TabIndex = 2
    TabOrder = 2
    object SettingsTabSheet: TTabSheet
      Caption = 'Settings'
      object Label3: TLabel
        Left = 8
        Top = 12
        Width = 110
        Height = 24
        AutoSize = False
        Caption = 'Login Command'
      end
      object Label2: TLabel
        Left = 8
        Top = 44
        Width = 110
        Height = 24
        AutoSize = False
        Caption = 'Config Command'
      end
      object Label4: TLabel
        Left = 8
        Top = 76
        Width = 225
        Height = 24
        AutoSize = False
        Caption = 'Node Display Interval (in seconds)'
      end
      object Label5: TLabel
        Left = 8
        Top = 108
        Width = 225
        Height = 24
        AutoSize = False
        Caption = 'Client Display Interval (in seconds)'
      end
      object PasswordLabel: TLabel
        Left = 8
        Top = 206
        Width = 110
        Height = 24
        AutoSize = False
        Caption = 'Password'
      end
      object LoginCmdEdit: TEdit
        Left = 120
        Top = 12
        Width = 169
        Height = 24
        Hint = 'Login command-line or URL (default = telnet://localhost)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object ConfigCmdEdit: TEdit
        Left = 120
        Top = 44
        Width = 169
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
        Height = 24
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
        Height = 24
        Associate = ClientIntEdit
        Min = 1
        Max = 99
        Position = 1
        TabOrder = 5
        Wrap = False
      end
      object UndockableCheckBox: TCheckBox
        Left = 8
        Top = 138
        Width = 281
        Height = 24
        Hint = 'Allow child windows to be "un-docked" from main window'
        Caption = 'Undockable Windows'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object TrayIconCheckBox: TCheckBox
        Left = 8
        Top = 170
        Width = 281
        Height = 24
        Hint = 'Create tray icon when minimized'
        Caption = 'Minimize to System Tray'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
        OnClick = TrayIconCheckBoxClick
      end
      object PasswordEdit: TEdit
        Left = 120
        Top = 206
        Width = 169
        Height = 24
        Hint = 'Required password for restoring from system tray icon'
        ParentShowHint = False
        PasswordChar = '*'
        ShowHint = True
        TabOrder = 8
      end
    end
    object CustomizeTabSheet: TTabSheet
      Caption = 'Customize'
      ImageIndex = 1
      object SourceComboBox: TComboBox
        Left = 8
        Top = 12
        Width = 135
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
        Left = 8
        Top = 44
        Width = 135
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
        Left = 8
        Top = 76
        Width = 135
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
        Left = 8
        Top = 12
        Width = 110
        Height = 24
        AutoSize = False
        Caption = 'Control Directory'
      end
      object Label6: TLabel
        Left = 8
        Top = 44
        Width = 110
        Height = 24
        AutoSize = False
        Caption = 'Hostname'
      end
      object Label7: TLabel
        Left = 8
        Top = 76
        Width = 110
        Height = 24
        AutoSize = False
        Caption = 'JavaScript Heap'
      end
      object CtrlDirEdit: TEdit
        Left = 120
        Top = 12
        Width = 169
        Height = 24
        Hint = 'Control directory (e.g. c:\sbbs\ctrl)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object HostnameEdit: TEdit
        Left = 120
        Top = 44
        Width = 169
        Height = 24
        Hint = 'Hostname (if different than configured in SCFG)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object JS_MaxBytesEdit: TEdit
        Left = 120
        Top = 76
        Width = 169
        Height = 24
        Hint = 
          'Maximum number of bytes that can be allocated for use by the Jav' +
          'aScript engine'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
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
