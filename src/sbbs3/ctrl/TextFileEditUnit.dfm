object TextFileEditForm: TTextFileEditForm
  Left = 317
  Top = 253
  Width = 783
  Height = 540
  Caption = 'TextFileEditForm'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -13
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  Icon.Data = {
    0000010001002020100000000000E80200001600000028000000200000004000
    0000010004000000000080020000000000000000000000000000000000000000
    000000008000008000000080800080000000800080008080000080808000C0C0
    C0000000FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF000000
    0000000000000000000000000000000000000000007777770000000000000000
    0000000000000007000000000000000000000000000000070000000000000000
    0000000000777007000000000000000000077070007770070000700000000000
    0077000700787000000007000000000007708000077877000070007000000000
    07088807777777770777000700000000008F88877FFFFF077887700700000000
    00088888F88888FF08870070000000000000880888877778F070007000000007
    77088888880007778F770077777000700008F088007777077F07000000700700
    008F08880800077778F7700000700708888F0880F08F807078F7777700700708
    F88F0780F070F07078F7887700700708888F0780F077807088F7777700700700
    008F0788FF00080888F77000007000000008F0780FFFF0088F77007000000000
    0008F07788000888887700700000000000008F07788888880870007000000000
    00088FF0077788088887000700000000008F888FF00000F87887700700000000
    0708F8088FFFFF88078700700000000007708000088888000070070000000000
    0077007000888007000070000000000000077700008F80070007000000000000
    0000000000888007000000000000000000000000000000070000000000000000
    000000000777777700000000000000000000000000000000000000000000FFFF
    FFFFFFFC0FFFFFFC0FFFFFF80FFFFFF80FFFFE180E7FFC00043FF800001FF800
    000FF800000FFC00001FFE00001FE0000001C000000180000001800000018000
    00018000000180000001FC00001FFC00001FFE00001FFC00000FF800000FF800
    001FF800003FFC180C7FFE380EFFFFF80FFFFFF80FFFFFF80FFFFFFFFFFF}
  Menu = Menu
  OldCreateOrder = False
  Position = poDefaultPosOnly
  OnShow = FormShow
  PixelsPerInch = 120
  TextHeight = 16
  object Panel: TPanel
    Left = 0
    Top = 0
    Width = 775
    Height = 33
    Align = alTop
    BevelOuter = bvNone
    TabOrder = 0
    object OkButton: TBitBtn
      Left = 10
      Top = 4
      Width = 90
      Height = 26
      Action = Save
      Caption = 'OK'
      ModalResult = 1
      TabOrder = 0
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        04000000000000010000120B0000120B00001000000000000000000000000000
        800000800000008080008000000080008000808000007F7F7F00BFBFBF000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00555555555555
        555555555555555555555555555555555555555555FF55555555555559055555
        55555555577FF5555555555599905555555555557777F5555555555599905555
        555555557777FF5555555559999905555555555777777F555555559999990555
        5555557777777FF5555557990599905555555777757777F55555790555599055
        55557775555777FF5555555555599905555555555557777F5555555555559905
        555555555555777FF5555555555559905555555555555777FF55555555555579
        05555555555555777FF5555555555557905555555555555777FF555555555555
        5990555555555555577755555555555555555555555555555555}
      NumGlyphs = 2
    end
    object CancelButton: TBitBtn
      Left = 105
      Top = 4
      Width = 89
      Height = 26
      Caption = 'Discard'
      ModalResult = 2
      TabOrder = 1
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        04000000000000010000120B0000120B00001000000000000000000000000000
        800000800000008080008000000080008000808000007F7F7F00BFBFBF000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333000000000
        3333333777777777F3333330F777777033333337F3F3F3F7F3333330F0808070
        33333337F7F7F7F7F3333330F080707033333337F7F7F7F7F3333330F0808070
        33333337F7F7F7F7F3333330F080707033333337F7F7F7F7F3333330F0808070
        333333F7F7F7F7F7F3F33030F080707030333737F7F7F7F7F7333300F0808070
        03333377F7F7F7F773333330F080707033333337F7F7F7F7F333333070707070
        33333337F7F7F7F7FF3333000000000003333377777777777F33330F88877777
        0333337FFFFFFFFF7F3333000000000003333377777777777333333330777033
        3333333337FFF7F3333333333000003333333333377777333333}
      NumGlyphs = 2
    end
  end
  object Memo: TRichEdit
    Left = 0
    Top = 33
    Width = 775
    Height = 451
    Align = alClient
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -15
    Font.Name = 'Courier'
    Font.Style = []
    HideSelection = False
    ParentFont = False
    PlainText = True
    PopupMenu = PopupMenu
    ScrollBars = ssBoth
    TabOrder = 1
    WantTabs = True
    WordWrap = False
  end
  object FindDialog: TFindDialog
    Options = [frDown, frHideUpDown]
    OnFind = FindDialogFind
    Left = 264
    Top = 80
  end
  object ReplaceDialog: TReplaceDialog
    OnFind = FindDialogFind
    OnReplace = ReplaceDialogReplace
    Left = 264
    Top = 112
  end
  object Menu: TMainMenu
    Left = 184
    Top = 80
    object FileMenuItem: TMenuItem
      Caption = '&File'
      object FileSaveMenuItem: TMenuItem
        Action = Save
      end
      object FileExitMenuItem: TMenuItem
        Caption = 'E&xit'
        OnClick = FileExitMenuItemClick
      end
    end
    object EditMenuItem: TMenuItem
      Caption = '&Edit'
      object EditUndoMenuItem: TMenuItem
        Action = Undo
      end
      object N5: TMenuItem
        Caption = '-'
      end
      object EditCutMenuItem: TMenuItem
        Action = Cut
      end
      object EditCopyMenuItem: TMenuItem
        Action = Copy
      end
      object EditPasteMenuItem: TMenuItem
        Action = Paste
      end
      object EditSelectAllMenuItem: TMenuItem
        Action = SelectAll
      end
      object N4: TMenuItem
        Caption = '-'
      end
      object EditFindMenuItem: TMenuItem
        Action = Find
      end
      object EditReplaceMenuItem: TMenuItem
        Action = Replace
      end
      object N3: TMenuItem
        Caption = '-'
      end
      object EditInsertCtrlA: TMenuItem
        Action = InsertCtrlA
      end
    end
    object FontMenuItem: TMenuItem
      Action = ChangeFont
    end
  end
  object PopupMenu: TPopupMenu
    Left = 184
    Top = 112
    object CutPopupMenuItem: TMenuItem
      Action = Cut
    end
    object CopyPopupMenuItem: TMenuItem
      Action = Copy
    end
    object PastePopupMenuItem: TMenuItem
      Action = Paste
    end
    object SelectAllPopupMenuItem: TMenuItem
      Action = SelectAll
    end
    object N1: TMenuItem
      Caption = '-'
    end
    object FindPopupMenuItem: TMenuItem
      Action = Find
    end
    object ReplacePopupMenuItem: TMenuItem
      Action = Replace
    end
    object N2: TMenuItem
      Caption = '-'
    end
    object CtrlAPopupMenuItem: TMenuItem
      Action = InsertCtrlA
    end
  end
  object ActionList: TActionList
    Left = 224
    Top = 80
    object InsertCtrlA: TAction
      Caption = '&Insert Ctrl-A'
      ShortCut = 16449
      OnExecute = InsertCtrlAExecute
    end
    object Find: TAction
      Caption = '&Find...'
      ShortCut = 16454
      OnExecute = FindExecute
    end
    object Replace: TAction
      Caption = '&Replace...'
      ShortCut = 16456
      OnExecute = ReplaceExecute
    end
    object SelectAll: TAction
      Caption = 'Select &All'
      OnExecute = SelectAllExecute
    end
    object Save: TAction
      Caption = '&Save'
      ShortCut = 16467
      OnExecute = SaveExecute
    end
    object Cut: TAction
      Caption = 'Cu&t'
      ShortCut = 16472
      OnExecute = CutExecute
    end
    object Copy: TAction
      Caption = '&Copy'
      ShortCut = 16451
      OnExecute = CopyExecute
    end
    object Paste: TAction
      Caption = '&Paste'
      ShortCut = 16470
      OnExecute = PasteExecute
    end
    object Undo: TAction
      Caption = '&Undo'
      ShortCut = 16474
      OnExecute = UndoExecute
    end
    object ChangeFont: TAction
      Caption = 'Font'
      OnExecute = ChangeFontExecute
    end
  end
  object FontDialog: TFontDialog
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    MinFontSize = 0
    MaxFontSize = 0
    Options = [fdApplyButton]
    OnApply = FontDialogApply
    Left = 144
    Top = 80
  end
end
