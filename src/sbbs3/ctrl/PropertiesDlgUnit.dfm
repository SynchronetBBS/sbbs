object PropertiesDlg: TPropertiesDlg
  Left = 496
  Top = 224
  BorderStyle = bsDialog
  Caption = 'Control Panel Properties'
  ClientHeight = 294
  ClientWidth = 433
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  ShowHint = True
  OnShow = FormShow
  DesignSize = (
    433
    294)
  PixelsPerInch = 120
  TextHeight = 16
  object Bevel1: TBevel
    Left = 10
    Top = 10
    Width = 306
    Height = 270
    Anchors = [akLeft, akTop, akRight, akBottom]
    Shape = bsFrame
  end
  object Label1: TLabel
    Left = 24
    Top = 88
    Width = 110
    Height = 24
    AutoSize = False
    Caption = 'Control Directory'
  end
  object Label2: TLabel
    Left = 24
    Top = 56
    Width = 110
    Height = 24
    AutoSize = False
    Caption = 'Config Command'
  end
  object Label3: TLabel
    Left = 24
    Top = 24
    Width = 110
    Height = 24
    AutoSize = False
    Caption = 'Login Command'
  end
  object Label4: TLabel
    Left = 24
    Top = 120
    Width = 225
    Height = 24
    AutoSize = False
    Caption = 'Node Display Interval (in seconds)'
  end
  object Label5: TLabel
    Left = 24
    Top = 152
    Width = 225
    Height = 24
    AutoSize = False
    Caption = 'Client Display Interval (in seconds)'
  end
  object PasswordLabel: TLabel
    Left = 24
    Top = 248
    Width = 110
    Height = 24
    AutoSize = False
    Caption = 'Password'
  end
  object OKBtn: TButton
    Left = 329
    Top = 10
    Width = 93
    Height = 31
    Anchors = [akTop, akRight]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 10
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
    TabOrder = 11
  end
  object CtrlDirEdit: TEdit
    Left = 136
    Top = 88
    Width = 169
    Height = 24
    Hint = 'Control directory (e.g. c:\sbbs\ctrl)'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 2
  end
  object ConfigCmdEdit: TEdit
    Left = 136
    Top = 56
    Width = 169
    Height = 24
    Hint = 'Configuration command line'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 1
  end
  object LoginCmdEdit: TEdit
    Left = 136
    Top = 24
    Width = 169
    Height = 24
    Hint = 'Login command-line or URL (default = telnet://localhost)'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
  end
  object TrayIconCheckBox: TCheckBox
    Left = 24
    Top = 214
    Width = 281
    Height = 24
    Hint = 'Create tray icon when minimized'
    Caption = 'Minimize to System Tray'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 8
    OnClick = TrayIconCheckBoxClick
  end
  object NodeIntEdit: TEdit
    Left = 256
    Top = 120
    Width = 25
    Height = 24
    Hint = 'Frequency of updates to Node window'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 3
    Text = '1'
  end
  object NodeIntUpDown: TUpDown
    Left = 281
    Top = 120
    Width = 19
    Height = 24
    Associate = NodeIntEdit
    Min = 1
    Max = 99
    Position = 1
    TabOrder = 4
    Wrap = False
  end
  object ClientIntEdit: TEdit
    Left = 256
    Top = 152
    Width = 25
    Height = 24
    Hint = 'Frequency of updates to clients window'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 5
    Text = '1'
  end
  object ClientIntUpDown: TUpDown
    Left = 281
    Top = 152
    Width = 19
    Height = 24
    Associate = ClientIntEdit
    Min = 1
    Max = 99
    Position = 1
    TabOrder = 6
    Wrap = False
  end
  object UndockableCheckBox: TCheckBox
    Left = 24
    Top = 182
    Width = 281
    Height = 24
    Hint = 'Allow child windows to be "un-docked" from main window'
    Caption = 'Undockable Windows'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 7
  end
  object PasswordEdit: TEdit
    Left = 136
    Top = 248
    Width = 169
    Height = 24
    Hint = 'Required password for restoring from system tray icon'
    ParentShowHint = False
    PasswordChar = '*'
    ShowHint = True
    TabOrder = 9
  end
end
