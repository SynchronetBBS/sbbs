object NodeForm: TNodeForm
  Left = 471
  Top = 304
  Width = 277
  Height = 214
  Caption = 'Nodes'
  Color = clBtnFace
  UseDockManager = True
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poDefault
  OnHide = FormHide
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object Toolbar: TToolBar
    Left = 0
    Top = 0
    Width = 269
    Height = 25
    Caption = 'Toolbar'
    EdgeBorders = []
    Flat = True
    Images = MainForm.ImageList
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
    object StartButton: TToolButton
      Left = 0
      Top = 0
      Action = MainForm.NodeListStart
    end
    object StopButton: TToolButton
      Left = 23
      Top = 0
      Action = MainForm.NodeListStop
    end
    object LockNodeButton: TToolButton
      Left = 46
      Top = 0
      Hint = 'Lock Node(s)'
      Caption = 'LockNodeButton'
      ImageIndex = 19
      OnClick = LockNodeButtonClick
    end
    object DownButton: TToolButton
      Left = 69
      Top = 0
      Hint = 'Down Node(s)'
      Caption = 'DownButton'
      ImageIndex = 21
      OnClick = DownButtonClick
    end
    object InterruptNodeButton: TToolButton
      Left = 92
      Top = 0
      Hint = 'Interrupt Node(s)'
      Caption = 'InterruptNodeButton'
      ImageIndex = 17
      OnClick = InterruptNodeButtonClick
    end
    object ClearErrorButton: TToolButton
      Left = 115
      Top = 0
      Hint = 'Clear Errors on Node(s)'
      Caption = 'ClearErrorButton'
      ImageIndex = 33
      OnClick = ClearErrorButtonClick
    end
    object ChatButton: TToolButton
      Left = 138
      Top = 0
      Hint = 'Chat with User'
      Caption = 'ChatButton'
      ImageIndex = 39
      OnClick = ChatButtonClick
    end
    object SpyButton: TToolButton
      Left = 161
      Top = 0
      Hint = 'Spy on User'
      Caption = 'SpyButton'
      ImageIndex = 41
      OnClick = SpyButtonClick
    end
  end
  object ListBox: TListBox
    Left = 0
    Top = 25
    Width = 269
    Height = 162
    Align = alClient
    ItemHeight = 13
    MultiSelect = True
    PopupMenu = PopupMenu
    TabOrder = 1
    OnKeyPress = ListBoxKeyPress
  end
  object Timer: TTimer
    OnTimer = TimerTick
    Left = 72
    Top = 120
  end
  object PopupMenu: TPopupMenu
    Images = MainForm.ImageList
    Left = 128
    Top = 72
    object LockMenuItem: TMenuItem
      Caption = 'Lock'
      ImageIndex = 19
      OnClick = LockNodeButtonClick
    end
    object DownMenuItem: TMenuItem
      Caption = 'Down'
      ImageIndex = 21
      OnClick = DownButtonClick
    end
    object InterruptMenuItem: TMenuItem
      Caption = 'Interrupt'
      ImageIndex = 17
      OnClick = InterruptNodeButtonClick
    end
    object ClearErrorsMenuItem: TMenuItem
      Caption = 'Clear Errors'
      ImageIndex = 33
      OnClick = ClearErrorButtonClick
    end
    object ChatMenuItem: TMenuItem
      Caption = 'Chat with User'
      ImageIndex = 39
      OnClick = ChatButtonClick
    end
    object SpyMenuItem: TMenuItem
      Caption = 'Spy on User'
      ImageIndex = 41
      OnClick = SpyButtonClick
    end
    object N1: TMenuItem
      Caption = '-'
    end
    object SelectAllMenuItem: TMenuItem
      Caption = 'Select &All'
      OnClick = SelectAllMenuItemClick
    end
  end
end
