object WebForm: TWebForm
  Left = 548
  Top = 684
  Width = 462
  Height = 229
  Caption = 'Web Server'
  Color = clBtnFace
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 13
  object ToolBar: TToolBar
    Left = 0
    Top = 0
    Width = 446
    Height = 25
    Caption = 'ToolBar'
    EdgeBorders = []
    Flat = True
    Images = MainForm.ImageList
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
    object StartButton: TToolButton
      Left = 0
      Top = 0
      Action = MainForm.WebStart
      Grouped = True
      ParentShowHint = False
      ShowHint = True
    end
    object LogPauseButton: TToolButton
      Left = 23
      Top = 0
      Action = MainForm.WebPause
      Style = tbsCheck
    end
    object StopButton: TToolButton
      Left = 46
      Top = 0
      Action = MainForm.WebStop
      Grouped = True
      ParentShowHint = False
      ShowHint = True
    end
    object RecycleButton: TToolButton
      Left = 69
      Top = 0
      Action = MainForm.WebRecycle
    end
    object ToolButton1: TToolButton
      Left = 92
      Top = 0
      Width = 8
      Caption = 'ToolButton1'
      ImageIndex = 1
      Style = tbsSeparator
    end
    object ConfigureButton: TToolButton
      Left = 100
      Top = 0
      Action = MainForm.WebConfigure
      ParentShowHint = False
      ShowHint = True
    end
    object ToolButton2: TToolButton
      Left = 123
      Top = 0
      Width = 8
      Caption = 'ToolButton2'
      ImageIndex = 5
      Style = tbsSeparator
    end
    object Status: TStaticText
      Left = 131
      Top = 0
      Width = 150
      Height = 22
      Hint = 'Web Server Status'
      Align = alClient
      AutoSize = False
      BorderStyle = sbsSunken
      Caption = 'Down'
      TabOrder = 0
    end
    object ToolButton3: TToolButton
      Left = 0
      Top = 0
      Width = 8
      Caption = 'ToolButton3'
      ImageIndex = 6
      Wrap = True
      Style = tbsSeparator
    end
    object ProgressBar: TProgressBar
      Left = 0
      Top = 30
      Width = 75
      Height = 22
      Hint = 'Web Server Utilization'
      Min = 0
      Max = 100
      Smooth = True
      Step = 1
      TabOrder = 1
    end
    object ToolButton4: TToolButton
      Left = 75
      Top = 30
      Width = 8
      Caption = 'ToolButton4'
      ImageIndex = 7
      Style = tbsSeparator
    end
    object LogLevelText: TStaticText
      Left = 83
      Top = 30
      Width = 75
      Height = 22
      Hint = 'Log Level'
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 2
    end
    object LogLevelUpDown: TUpDown
      Left = 158
      Top = 30
      Width = 16
      Height = 22
      Hint = 'Log Level Adjustment'
      Min = 0
      Max = 7
      Position = 0
      TabOrder = 3
      Wrap = False
      OnChangingEx = LogLevelUpDownChangingEx
    end
  end
  object Log: TRichEdit
    Left = 0
    Top = 25
    Width = 446
    Height = 165
    Align = alClient
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    HideScrollBars = False
    ParentFont = False
    PopupMenu = MainForm.LogPopupMenu
    ReadOnly = True
    ScrollBars = ssBoth
    TabOrder = 1
    WordWrap = False
  end
end
