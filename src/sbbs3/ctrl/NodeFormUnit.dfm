object NodeForm: TNodeForm
  Left = 579
  Top = 396
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
    object RerunToolButton: TToolButton
      Left = 115
      Top = 0
      Hint = 'Rerun Node(s)'
      Caption = 'RerunToolButton'
      ImageIndex = 55
      OnClick = RerunNodeButtonClick
    end
    object ClearErrorButton: TToolButton
      Left = 138
      Top = 0
      Hint = 'Clear Errors on Node(s)'
      Caption = 'ClearErrorButton'
      ImageIndex = 31
      OnClick = ClearErrorButtonClick
    end
    object SpyButton: TToolButton
      Left = 161
      Top = 0
      Hint = 'Spy on Node(s)'
      Caption = 'SpyButton'
      ImageIndex = 39
      OnClick = SpyButtonClick
    end
    object ChatButton: TToolButton
      Left = 184
      Top = 0
      Hint = 'Chat with User'
      Caption = 'ChatButton'
      ImageIndex = 37
      OnClick = ChatButtonClick
    end
    object UserMsgButton: TToolButton
      Left = 207
      Top = 0
      Hint = 'Send Message to User'
      Caption = 'UserMsgButton'
      ImageIndex = 57
      OnClick = UserMsgButtonClick
    end
    object UserEditButton: TToolButton
      Left = 230
      Top = 0
      Hint = 'Edit User'
      Caption = 'UserEditButton'
      ImageIndex = 27
      OnClick = UserEditButtonClick
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
    Left = 128
    Top = 72
    object LockMenuItem: TMenuItem
      Caption = 'Lock Node'
      ImageIndex = 19
      OnClick = LockNodeButtonClick
    end
    object DownMenuItem: TMenuItem
      Caption = 'Down Node'
      ImageIndex = 21
      OnClick = DownButtonClick
    end
    object RerunMenuItem: TMenuItem
      Caption = 'Rerun Node'
      ImageIndex = 55
      OnClick = RerunNodeButtonClick
    end
    object InterruptMenuItem: TMenuItem
      Caption = 'Interrupt Node'
      ImageIndex = 17
      OnClick = InterruptNodeButtonClick
    end
    object ClearErrorsMenuItem: TMenuItem
      Caption = 'Clear Errors'
      ImageIndex = 33
      OnClick = ClearErrorButtonClick
    end
    object ChatMenuItem: TMenuItem
      Caption = 'Chat w/User'
      ImageIndex = 39
      OnClick = ChatButtonClick
    end
    object SendMsgMenuItem: TMenuItem
      Caption = 'Send User Msg'
      OnClick = UserMsgButtonClick
    end
    object SpyMenuItem: TMenuItem
      Caption = 'Spy on Node'
      ImageIndex = 41
      OnClick = SpyButtonClick
    end
    object EditUser1: TMenuItem
      Caption = 'Edit User'
      Hint = 'EditUserMenuItem'
      ImageIndex = 27
      OnClick = UserEditButtonClick
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
