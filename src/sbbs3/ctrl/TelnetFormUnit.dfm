object TelnetForm: TTelnetForm
  Left = 596
  Top = 341
  Width = 350
  Height = 214
  Caption = 'Telnet Server'
  Color = clBtnFace
  UseDockManager = True
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -14
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poDefault
  OnHide = FormHide
  OnShow = FormShow
  PixelsPerInch = 120
  TextHeight = 16
  object ToolBar: TToolBar
    Left = 0
    Top = 0
    Width = 342
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
      Action = MainForm.TelnetStart
      Grouped = True
      ParentShowHint = False
      ShowHint = True
    end
    object StopButton: TToolButton
      Left = 23
      Top = 0
      Action = MainForm.TelnetStop
      Grouped = True
      ParentShowHint = False
      ShowHint = True
    end
    object RecycleButton: TToolButton
      Left = 46
      Top = 0
      Action = MainForm.TelnetRecycle
    end
    object ToolButton1: TToolButton
      Left = 69
      Top = 0
      Width = 8
      Caption = 'ToolButton1'
      ImageIndex = 1
      Style = tbsSeparator
    end
    object ConfigureButton: TToolButton
      Left = 77
      Top = 0
      Action = MainForm.TelnetConfigure
      ParentShowHint = False
      ShowHint = True
    end
    object ToolButton2: TToolButton
      Left = 100
      Top = 0
      Width = 8
      Caption = 'ToolButton2'
      ImageIndex = 5
      Style = tbsSeparator
    end
    object Status: TStaticText
      Left = 108
      Top = 0
      Width = 150
      Height = 22
      Hint = 'Telnet Server Status'
      Align = alClient
      AutoSize = False
      BorderStyle = sbsSunken
      Caption = 'Status'
      TabOrder = 0
    end
    object ToolButton3: TToolButton
      Left = 258
      Top = 0
      Width = 8
      Caption = 'ToolButton3'
      ImageIndex = 6
      Style = tbsSeparator
    end
    object ProgressBar: TProgressBar
      Left = 266
      Top = 0
      Width = 75
      Height = 22
      Hint = 'Telnet Server Utilization'
      Min = 0
      Max = 100
      Smooth = True
      Step = 1
      TabOrder = 1
    end
  end
  object Log: TMemo
    Left = 0
    Top = 25
    Width = 342
    Height = 156
    Align = alClient
    ReadOnly = True
    ScrollBars = ssBoth
    TabOrder = 1
  end
end
