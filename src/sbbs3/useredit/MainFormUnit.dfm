object Form1: TForm1
  Left = 440
  Top = 173
  ActiveControl = ScrollBar
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = 'Synchronet User Editor'
  ClientHeight = 442
  ClientWidth = 550
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -14
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  Menu = MainMenu
  OldCreateOrder = False
  Position = poDefaultPosOnly
  OnClose = FormClose
  OnShow = FormShow
  PixelsPerInch = 120
  TextHeight = 16
  object Panel: TPanel
    Left = 0
    Top = 0
    Width = 550
    Height = 442
    Align = alClient
    BevelOuter = bvNone
    TabOrder = 0
    object PageControl: TPageControl
      Left = 0
      Top = 94
      Width = 550
      Height = 348
      ActivePage = MsgFileSettingsTabSheet
      Align = alClient
      TabOrder = 0
      object PersonalTabSheet: TTabSheet
        Caption = 'Personal'
        object Label2: TLabel
          Left = 10
          Top = 10
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Real Name'
        end
        object Label1: TLabel
          Left = 10
          Top = 39
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Computer'
        end
        object NetmailAddress: TLabel
          Left = 10
          Top = 69
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'NetMail'
        end
        object Label7: TLabel
          Left = 10
          Top = 128
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Note'
        end
        object HandleLabel: TLabel
          Left = 364
          Top = 69
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Handle'
        end
        object Label5: TLabel
          Left = 364
          Top = 128
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Password'
        end
        object Label6: TLabel
          Left = 364
          Top = 98
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Birthdate'
        end
        object Label8: TLabel
          Left = 364
          Top = 39
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Connection'
        end
        object Label16: TLabel
          Left = 364
          Top = 10
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Gender'
        end
        object Label18: TLabel
          Left = 10
          Top = 98
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Phone'
        end
        object Label24: TLabel
          Left = 10
          Top = 158
          Width = 80
          Height = 25
          AutoSize = False
          Caption = 'Comment'
        end
        object NameEdit: TEdit
          Left = 98
          Top = 10
          Width = 248
          Height = 24
          TabOrder = 0
          OnChange = EditChange
        end
        object ComputerEdit: TEdit
          Left = 98
          Top = 39
          Width = 248
          Height = 24
          TabOrder = 1
          OnChange = EditChange
        end
        object NetmailEdit: TEdit
          Left = 98
          Top = 69
          Width = 248
          Height = 24
          TabOrder = 2
          OnChange = EditChange
        end
        object NoteEdit: TEdit
          Left = 98
          Top = 128
          Width = 248
          Height = 24
          TabOrder = 4
          OnChange = EditChange
        end
        object HandleEdit: TEdit
          Left = 453
          Top = 69
          Width = 70
          Height = 24
          TabOrder = 8
          OnChange = EditChange
        end
        object PasswordEdit: TEdit
          Left = 453
          Top = 128
          Width = 70
          Height = 24
          CharCase = ecUpperCase
          PasswordChar = '*'
          TabOrder = 10
          OnChange = EditChange
        end
        object BirthdateEdit: TEdit
          Left = 453
          Top = 98
          Width = 70
          Height = 24
          TabOrder = 9
          OnChange = EditChange
        end
        object ModemEdit: TEdit
          Left = 453
          Top = 39
          Width = 70
          Height = 24
          TabOrder = 7
          OnChange = EditChange
        end
        object SexEdit: TEdit
          Left = 453
          Top = 10
          Width = 21
          Height = 24
          TabOrder = 6
          OnChange = EditChange
        end
        object PhoneEdit: TEdit
          Left = 98
          Top = 98
          Width = 248
          Height = 24
          TabOrder = 3
          OnChange = EditChange
        end
        object CommentEdit: TEdit
          Left = 98
          Top = 158
          Width = 425
          Height = 24
          TabOrder = 5
          OnChange = EditChange
        end
        object GroupBox9: TGroupBox
          Left = 89
          Top = 197
          Width = 365
          Height = 90
          Caption = 'Address'
          TabOrder = 11
          object AddressEdit: TEdit
            Left = 10
            Top = 25
            Width = 247
            Height = 24
            Anchors = [akLeft, akTop, akRight]
            TabOrder = 0
            OnChange = EditChange
          end
          object LocationEdit: TEdit
            Left = 10
            Top = 54
            Width = 247
            Height = 24
            Anchors = [akLeft, akTop, akRight]
            TabOrder = 1
            OnChange = EditChange
          end
          object ZipCodeEdit: TEdit
            Left = 266
            Top = 54
            Width = 90
            Height = 24
            Anchors = [akTop, akRight]
            TabOrder = 2
            OnChange = EditChange
          end
        end
      end
      object SecurityTabSheet: TTabSheet
        Caption = 'Security'
        ImageIndex = 1
        object Label10: TLabel
          Left = 10
          Top = 10
          Width = 50
          Height = 26
          AutoSize = False
          Caption = 'Level'
        end
        object Label11: TLabel
          Left = 138
          Top = 10
          Width = 70
          Height = 26
          AutoSize = False
          Caption = 'Expiration'
        end
        object Label25: TLabel
          Left = 10
          Top = 226
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Exemptions'
        end
        object Label26: TLabel
          Left = 10
          Top = 256
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Restrictions'
        end
        object Label27: TLabel
          Left = 325
          Top = 10
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Credits'
        end
        object Label28: TLabel
          Left = 325
          Top = 69
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Minutes'
        end
        object Label29: TLabel
          Left = 325
          Top = 39
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Free Credits'
        end
        object LevelEdit: TEdit
          Left = 59
          Top = 10
          Width = 31
          Height = 24
          MaxLength = 2
          TabOrder = 0
          OnChange = EditChange
        end
        object ExpireEdit: TEdit
          Left = 217
          Top = 10
          Width = 70
          Height = 24
          TabOrder = 1
          OnChange = EditChange
        end
        object ExemptionsEdit: TEdit
          Left = 98
          Top = 226
          Width = 248
          Height = 24
          CharCase = ecUpperCase
          TabOrder = 3
          OnChange = EditChange
        end
        object RestrictionsEdit: TEdit
          Left = 98
          Top = 256
          Width = 248
          Height = 24
          CharCase = ecUpperCase
          TabOrder = 4
          OnChange = EditChange
        end
        object CreditsEdit: TEdit
          Left = 414
          Top = 10
          Width = 109
          Height = 24
          CharCase = ecUpperCase
          MaxLength = 26
          TabOrder = 5
          OnChange = EditChange
        end
        object MinutesEdit: TEdit
          Left = 414
          Top = 69
          Width = 109
          Height = 24
          CharCase = ecUpperCase
          MaxLength = 26
          TabOrder = 7
          OnChange = EditChange
        end
        object FreeCreditsEdit: TEdit
          Left = 414
          Top = 39
          Width = 109
          Height = 24
          CharCase = ecUpperCase
          MaxLength = 26
          TabOrder = 6
          OnChange = EditChange
        end
        object GroupBox8: TGroupBox
          Left = 10
          Top = 49
          Width = 287
          Height = 159
          Caption = 'Flag Sets'
          TabOrder = 2
          object Label12: TLabel
            Left = 10
            Top = 30
            Width = 21
            Height = 25
            AutoSize = False
            Caption = '1'
          end
          object Label13: TLabel
            Left = 10
            Top = 59
            Width = 21
            Height = 26
            AutoSize = False
            Caption = '2'
          end
          object Label14: TLabel
            Left = 10
            Top = 89
            Width = 21
            Height = 25
            AutoSize = False
            Caption = '3'
          end
          object Label15: TLabel
            Left = 10
            Top = 118
            Width = 21
            Height = 26
            AutoSize = False
            Caption = '4'
          end
          object Flags1Edit: TEdit
            Left = 30
            Top = 30
            Width = 247
            Height = 24
            CharCase = ecUpperCase
            TabOrder = 0
            OnChange = EditChange
          end
          object Flags2Edit: TEdit
            Left = 30
            Top = 59
            Width = 247
            Height = 24
            CharCase = ecUpperCase
            TabOrder = 1
            OnChange = EditChange
          end
          object Flags3Edit: TEdit
            Left = 30
            Top = 89
            Width = 247
            Height = 24
            CharCase = ecUpperCase
            TabOrder = 2
            OnChange = EditChange
          end
          object Flags4Edit: TEdit
            Left = 30
            Top = 118
            Width = 247
            Height = 24
            CharCase = ecUpperCase
            TabOrder = 3
            OnChange = EditChange
          end
        end
      end
      object StatsTabSheet: TTabSheet
        Caption = 'Statistics'
        ImageIndex = 2
        object GroupBox1: TGroupBox
          Left = 187
          Top = 148
          Width = 169
          Height = 149
          Caption = 'Time On'
          TabOrder = 0
          object Label22: TLabel
            Left = 10
            Top = 25
            Width = 74
            Height = 25
            AutoSize = False
            Caption = 'Total'
          end
          object Label23: TLabel
            Left = 10
            Top = 54
            Width = 74
            Height = 26
            AutoSize = False
            Caption = 'Today'
          end
          object Label30: TLabel
            Left = 10
            Top = 113
            Width = 74
            Height = 26
            AutoSize = False
            Caption = 'Extra'
          end
          object Label31: TLabel
            Left = 10
            Top = 84
            Width = 74
            Height = 26
            AutoSize = False
            Caption = 'Last Call'
          end
          object TimeOnEdit: TEdit
            Left = 89
            Top = 25
            Width = 70
            Height = 24
            Anchors = [akTop, akRight]
            TabOrder = 0
            OnChange = EditChange
          end
          object TimeOnTodayEdit: TEdit
            Left = 89
            Top = 54
            Width = 70
            Height = 24
            Anchors = [akTop, akRight]
            TabOrder = 1
            OnChange = EditChange
          end
          object ExtraTimeEdit: TEdit
            Left = 89
            Top = 113
            Width = 70
            Height = 24
            Anchors = [akTop, akRight]
            TabOrder = 2
            OnChange = EditChange
          end
          object LastCallTimeEdit: TEdit
            Left = 89
            Top = 84
            Width = 70
            Height = 24
            Anchors = [akTop, akRight]
            TabOrder = 3
            OnChange = EditChange
          end
        end
        object GroupBox2: TGroupBox
          Left = 10
          Top = 108
          Width = 168
          Height = 90
          Caption = 'Logons'
          TabOrder = 1
          object Label20: TLabel
            Left = 10
            Top = 25
            Width = 70
            Height = 25
            AutoSize = False
            Caption = 'Total'
          end
          object Label21: TLabel
            Left = 10
            Top = 54
            Width = 70
            Height = 26
            AutoSize = False
            Caption = 'Today'
          end
          object LogonsEdit: TEdit
            Left = 89
            Top = 25
            Width = 70
            Height = 24
            TabOrder = 0
            OnChange = EditChange
          end
          object LogonsTodayEdit: TEdit
            Left = 89
            Top = 54
            Width = 70
            Height = 24
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object GroupBox3: TGroupBox
          Left = 10
          Top = 10
          Width = 168
          Height = 90
          Caption = 'Dates'
          TabOrder = 2
          object Label17: TLabel
            Left = 10
            Top = 25
            Width = 74
            Height = 25
            AutoSize = False
            Caption = 'First on'
          end
          object Label19: TLabel
            Left = 10
            Top = 54
            Width = 74
            Height = 26
            AutoSize = False
            Caption = 'Last on'
          end
          object FirstOnEdit: TEdit
            Left = 89
            Top = 25
            Width = 70
            Height = 24
            TabOrder = 0
            OnChange = EditChange
          end
          object LastOnEdit: TEdit
            Left = 89
            Top = 54
            Width = 70
            Height = 24
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object GroupBox4: TGroupBox
          Left = 10
          Top = 207
          Width = 168
          Height = 90
          Caption = 'Posts'
          TabOrder = 3
          object Label32: TLabel
            Left = 10
            Top = 25
            Width = 70
            Height = 25
            AutoSize = False
            Caption = 'Total'
          end
          object Label33: TLabel
            Left = 10
            Top = 54
            Width = 70
            Height = 26
            AutoSize = False
            Caption = 'Today'
          end
          object PostsTotalEdit: TEdit
            Left = 89
            Top = 25
            Width = 70
            Height = 24
            TabOrder = 0
            OnChange = EditChange
          end
          object PostsTodayEdit: TEdit
            Left = 89
            Top = 54
            Width = 70
            Height = 24
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object GroupBox5: TGroupBox
          Left = 364
          Top = 177
          Width = 169
          Height = 120
          Caption = 'E-mail'
          TabOrder = 4
          object Label34: TLabel
            Left = 10
            Top = 25
            Width = 70
            Height = 25
            AutoSize = False
            Caption = 'Total'
          end
          object Label35: TLabel
            Left = 10
            Top = 54
            Width = 70
            Height = 26
            AutoSize = False
            Caption = 'Today'
          end
          object Label36: TLabel
            Left = 10
            Top = 84
            Width = 70
            Height = 26
            AutoSize = False
            Caption = 'To Sysop'
          end
          object EmailTotalEdit: TEdit
            Left = 89
            Top = 25
            Width = 70
            Height = 24
            TabOrder = 0
            OnChange = EditChange
          end
          object EmailTodayEdit: TEdit
            Left = 89
            Top = 54
            Width = 70
            Height = 24
            TabOrder = 1
            OnChange = EditChange
          end
          object FeedbackEdit: TEdit
            Left = 89
            Top = 84
            Width = 70
            Height = 24
            TabOrder = 2
            OnChange = EditChange
          end
        end
        object GroupBox6: TGroupBox
          Left = 187
          Top = 10
          Width = 169
          Height = 90
          Caption = 'Uploads'
          TabOrder = 5
          object Label37: TLabel
            Left = 10
            Top = 25
            Width = 70
            Height = 25
            AutoSize = False
            Caption = 'Total'
          end
          object Label38: TLabel
            Left = 10
            Top = 54
            Width = 70
            Height = 26
            AutoSize = False
            Caption = 'Bytes'
          end
          object UploadedFilesEdit: TEdit
            Left = 89
            Top = 25
            Width = 70
            Height = 24
            TabOrder = 0
            OnChange = EditChange
          end
          object UploadedBytesEdit: TEdit
            Left = 89
            Top = 54
            Width = 70
            Height = 24
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object GroupBox7: TGroupBox
          Left = 364
          Top = 10
          Width = 169
          Height = 119
          Caption = 'Downloads'
          TabOrder = 6
          object Label39: TLabel
            Left = 10
            Top = 25
            Width = 70
            Height = 25
            AutoSize = False
            Caption = 'Total'
          end
          object Label40: TLabel
            Left = 10
            Top = 54
            Width = 70
            Height = 26
            AutoSize = False
            Caption = 'Bytes'
          end
          object Label42: TLabel
            Left = 10
            Top = 84
            Width = 70
            Height = 26
            AutoSize = False
            Caption = 'Leech'
          end
          object DownloadedFilesEdit: TEdit
            Left = 89
            Top = 25
            Width = 70
            Height = 24
            TabOrder = 0
            OnChange = EditChange
          end
          object DownloadedBytesEdit: TEdit
            Left = 89
            Top = 54
            Width = 70
            Height = 24
            TabOrder = 1
            OnChange = EditChange
          end
          object LeechEdit: TEdit
            Left = 89
            Top = 84
            Width = 70
            Height = 24
            TabOrder = 2
            OnChange = EditChange
          end
        end
      end
      object SettingsTabSheet: TTabSheet
        Caption = 'Settings'
        ImageIndex = 3
        object GroupBox10: TGroupBox
          Left = 10
          Top = 10
          Width = 257
          Height = 188
          Caption = 'Terminal'
          TabOrder = 0
          object Label3: TLabel
            Left = 167
            Top = 148
            Width = 50
            Height = 26
            AutoSize = False
            Caption = 'Rows'
          end
          object RowsEdit: TEdit
            Left = 217
            Top = 148
            Width = 30
            Height = 24
            MaxLength = 2
            TabOrder = 0
            OnChange = EditChange
          end
          object TerminalCheckListBox: TCheckListBox
            Left = 10
            Top = 30
            Width = 237
            Height = 109
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 16
            Items.Strings = (
              'Auto-Detect'
              'Extended ASCII'
              'ANSI'
              'Color'
              'RIP'
              'WIP'
              'Pause'
              'Hot Keys'
              'Spinning Cursor')
            TabOrder = 1
          end
        end
        object GroupBox11: TGroupBox
          Left = 276
          Top = 10
          Width = 257
          Height = 139
          Caption = 'Logon'
          TabOrder = 1
          object LogonCheckListBox: TCheckListBox
            Left = 10
            Top = 30
            Width = 237
            Height = 89
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 16
            Items.Strings = (
              'Ask for New Message Scan'
              'Ask for Your Message Scan'
              'Remember Current Sub-board'
              'Quiet Mode (Q exempt)'
              'Auto-Logon via IP (V exempt)')
            TabOrder = 0
          end
        end
        object GroupBox13: TGroupBox
          Left = 276
          Top = 158
          Width = 257
          Height = 139
          Caption = 'Chat'
          TabOrder = 2
          object ChatCheckListBox: TCheckListBox
            Left = 10
            Top = 30
            Width = 237
            Height = 89
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 16
            Items.Strings = (
              'Multinode Chat Echo'
              'Multinode Chat Actions'
              'Available for Internode Chat'
              'Multinode Activity Alerts'
              'Split-screen Private Chat')
            TabOrder = 0
          end
        end
        object GroupBox15: TGroupBox
          Left = 10
          Top = 207
          Width = 257
          Height = 90
          Caption = 'Command Shell'
          TabOrder = 3
          object ShellEdit: TEdit
            Left = 10
            Top = 30
            Width = 90
            Height = 24
            CharCase = ecUpperCase
            TabOrder = 0
            OnChange = EditChange
          end
          object ExpertCheckBox: TCheckBox
            Left = 10
            Top = 59
            Width = 198
            Height = 21
            Caption = 'Expert Menu Mode'
            TabOrder = 1
            OnClick = EditChange
          end
        end
      end
      object MsgFileSettingsTabSheet: TTabSheet
        Caption = 'Msg/File Settings'
        ImageIndex = 4
        object GroupBox16: TGroupBox
          Left = 10
          Top = 10
          Width = 257
          Height = 119
          Caption = 'Message'
          TabOrder = 0
          object Label41: TLabel
            Left = 59
            Top = 79
            Width = 100
            Height = 26
            AutoSize = False
            Caption = 'External Editor'
          end
          object MessageCheckListBox: TCheckListBox
            Left = 10
            Top = 30
            Width = 237
            Height = 40
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 16
            Items.Strings = (
              'Forward Email to NetMail'
              'Clear Screen Between Messages')
            TabOrder = 0
          end
          object EditorEdit: TEdit
            Left = 158
            Top = 79
            Width = 89
            Height = 24
            Anchors = [akLeft, akTop, akRight]
            CharCase = ecUpperCase
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object GroupBox12: TGroupBox
          Left = 276
          Top = 10
          Width = 257
          Height = 188
          Caption = 'File'
          TabOrder = 1
          object Label4: TLabel
            Left = 59
            Top = 148
            Width = 149
            Height = 26
            Anchors = [akLeft, akTop, akBottom]
            AutoSize = False
            Caption = 'Temp/QWK File Type'
          end
          object Label9: TLabel
            Left = 59
            Top = 108
            Width = 169
            Height = 26
            Anchors = [akLeft, akTop, akBottom]
            AutoSize = False
            Caption = 'Default Download Protocol'
          end
          object TempFileExtEdit: TEdit
            Left = 207
            Top = 148
            Width = 40
            Height = 24
            Anchors = [akLeft, akTop, akBottom]
            CharCase = ecUpperCase
            MaxLength = 3
            TabOrder = 0
            OnChange = EditChange
          end
          object ProtocolEdit: TEdit
            Left = 226
            Top = 108
            Width = 21
            Height = 24
            Anchors = [akLeft, akTop, akBottom]
            CharCase = ecUpperCase
            MaxLength = 1
            TabOrder = 1
            OnChange = EditChange
          end
          object FileCheckListBox: TCheckListBox
            Left = 10
            Top = 30
            Width = 237
            Height = 70
            OnClickCheck = EditChange
            Anchors = [akLeft, akRight, akBottom]
            ItemHeight = 16
            Items.Strings = (
              'Auto-New Scan'
              'Extended Descriptions'
              'Batch Flagging'
              'Auto-Hangup After Transfer')
            TabOrder = 2
          end
        end
        object GroupBox14: TGroupBox
          Left = 10
          Top = 138
          Width = 257
          Height = 159
          Caption = 'QWK Message Packet'
          TabOrder = 2
          object QWKCheckListBox: TCheckListBox
            Left = 10
            Top = 30
            Width = 237
            Height = 109
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 16
            Items.Strings = (
              'Include New Files List'
              'Include Unread Email'
              'Include ALL Email'
              'Delete Email After Download'
              'Include Messages From Self'
              'Expand Ctrl-A Codes to ANSI'
              'Strip Ctrl-A codes'
              'Include File Attachments'
              'Include Index Files'
              'Include Time Zone (@TZ)'
              'Include Seen-Bys (@VIA)'
              'Extraneous Control Files')
            TabOrder = 0
          end
        end
      end
      object ExtendedCommentTabSheet: TTabSheet
        Caption = 'Extended Comment'
        ImageIndex = 5
        object Memo: TMemo
          Left = 10
          Top = 10
          Width = 523
          Height = 287
          ScrollBars = ssBoth
          TabOrder = 0
          OnChange = EditChange
        end
      end
    end
    object TopPanel: TPanel
      Left = 0
      Top = 44
      Width = 550
      Height = 50
      Align = alTop
      BevelOuter = bvNone
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      object NumberLabel: TLabel
        Left = 354
        Top = 10
        Width = 61
        Height = 26
        AutoSize = False
        Caption = 'Number'
      end
      object AliasLabel: TLabel
        Left = 10
        Top = 10
        Width = 40
        Height = 26
        AutoSize = False
        Caption = 'Alias'
      end
      object NumberEdit: TEdit
        Left = 414
        Top = 10
        Width = 50
        Height = 24
        TabOrder = 1
        OnKeyPress = NumberEditKeyPress
      end
      object TotalStaticText: TStaticText
        Left = 473
        Top = 10
        Width = 70
        Height = 26
        AutoSize = False
        BorderStyle = sbsSunken
        TabOrder = 2
      end
      object AliasEdit: TEdit
        Left = 49
        Top = 10
        Width = 198
        Height = 24
        TabOrder = 0
        OnChange = EditChange
      end
    end
    object ToolBar: TToolBar
      Left = 0
      Top = 0
      Width = 550
      Height = 25
      ButtonHeight = 24
      EdgeBorders = [ebTop, ebBottom]
      EdgeOuter = esNone
      Flat = True
      Images = ImageList
      Indent = 4
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      object ToolButton2: TToolButton
        Left = 4
        Top = 0
        Action = NewUser
      end
      object ToolButton1: TToolButton
        Left = 27
        Top = 0
        Action = SaveUser
      end
      object ToolButton5: TToolButton
        Left = 50
        Top = 0
        Action = RefreshUser
      end
      object ToolButton3: TToolButton
        Left = 73
        Top = 0
        Action = DeleteUser
      end
      object ToolButton4: TToolButton
        Left = 96
        Top = 0
        Action = DeactivateUser
      end
      object ToolButton6: TToolButton
        Left = 119
        Top = 0
        Width = 7
        Caption = 'ToolButton6'
        ImageIndex = 11
        Style = tbsSeparator
      end
      object FindEdit: TEdit
        Left = 126
        Top = 0
        Width = 121
        Height = 24
        TabOrder = 0
        OnKeyPress = FindEditKeyPress
      end
      object FindButton: TButton
        Left = 247
        Top = 0
        Width = 42
        Height = 24
        Caption = 'Find'
        TabOrder = 2
        OnClick = FindButtonClick
      end
      object FindNextButton: TButton
        Left = 289
        Top = 0
        Width = 42
        Height = 24
        Caption = 'Next'
        TabOrder = 1
        OnClick = FindButtonClick
      end
      object ToolButton7: TToolButton
        Left = 331
        Top = 0
        Width = 8
        Caption = 'ToolButton7'
        ImageIndex = 12
        Style = tbsSeparator
      end
      object Status: TEdit
        Left = 339
        Top = 0
        Width = 102
        Height = 24
        Color = clMenu
        ReadOnly = True
        TabOrder = 3
      end
    end
    object ScrollBar: TScrollBar
      Left = 0
      Top = 25
      Width = 550
      Height = 19
      Align = alTop
      LargeChange = 10
      Min = 1
      PageSize = 0
      Position = 1
      TabOrder = 3
      OnChange = ScrollBarChange
    end
  end
  object MainMenu: TMainMenu
    Images = ImageList
    Left = 272
    Top = 32
    object FileMenuItem: TMenuItem
      Caption = '&File'
      Hint = 'Flag Current User Record as Deleted'
      object FileNewUserMenuItem: TMenuItem
        Action = NewUser
      end
      object FileSaveUserMenuItem: TMenuItem
        Action = SaveUser
      end
      object FileRefreshUserMenuItem: TMenuItem
        Action = RefreshUser
      end
      object FileDeleteMenuItem: TMenuItem
        Action = DeleteUser
        Hint = 'Toggle Deleted User Status'
      end
      object FileDeactivateUserMenuItem: TMenuItem
        Action = DeactivateUser
      end
      object N1: TMenuItem
        Caption = '-'
      end
      object FileExitMenuItem: TMenuItem
        Caption = 'E&xit'
        OnClick = FileExitMenuItemClick
      end
    end
  end
  object ActionList: TActionList
    Images = ImageList
    Left = 241
    Top = 33
    object SaveUser: TAction
      Caption = '&Save User'
      Enabled = False
      Hint = 'Save Current User Record'
      ImageIndex = 2
      ShortCut = 16467
      OnExecute = SaveUserExecute
    end
    object NewUser: TAction
      Caption = '&New User'
      Hint = 'Create New User Record'
      ImageIndex = 0
      ShortCut = 16462
      OnExecute = NewUserExecute
    end
    object RefreshUser: TAction
      Caption = '&Reload User'
      Hint = 'Reload User Record'
      ImageIndex = 4
      ShortCut = 116
      OnExecute = ScrollBarChange
    end
    object DeleteUser: TAction
      Caption = '&Delete/Undelete User'
      Hint = 'Flag Current User Record as Deleted'
      ImageIndex = 6
      ShortCut = 16452
      OnExecute = DeleteUserExecute
    end
    object DeactivateUser: TAction
      Caption = 'De&activate/Reactivate User'
      Hint = 'Deactivate/Reactive User Record'
      ImageIndex = 8
      ShortCut = 16449
      OnExecute = DeactivateUserExecute
    end
  end
  object ImageList: TImageList
    Left = 217
    Top = 33
    Bitmap = {
      494C01010C000E00040010001000FFFFFFFFFF10FFFFFFFFFFFFFFFF424D3600
      0000000000003600000028000000400000004000000001002000000000000040
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      000000000000FFFFFF007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B000000
      0000FFFFFF00FFFFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000FFFFFF00FFFFFF000000000000000000000000000000
      00000000000000000000BDBDBD00BDBDBD00BDBDBD00BDBDBD00BDBDBD000000
      0000000000000000000000000000000000000000000000000000000000000000
      00007B7B7B007B7B7B00000000000000000000000000FFFFFF00000000007B7B
      7B007B7B7B0000000000FFFFFF00000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000000000000000000000000000FFFFFF00FFFFFF000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000000000000000
      0000000000007B7B7B007B7B7B007B7B7B000000000000000000000000000000
      00007B7B7B007B7B7B00BDBDBD007B7B7B00000000007B7B7B00BDBDBD007B7B
      7B007B7B7B000000000000000000000000000000000000000000000000007B7B
      7B00000000000000000000000000000000007B7B7B00FFFFFF00000000000000
      0000000000007B7B7B0000000000FFFFFF000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B00FFFFFF007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B0000000000FFFFFF00000000000000
      0000000000007B7B7B007B7B7B007B7B7B00000000000000000000000000BDBD
      BD00BDBDBD00BDBDBD00BDBDBD007B7B7B00000000007B7B7B00BDBDBD00BDBD
      BD00BDBDBD00BDBDBD00000000000000000000000000000000007B7B7B00FFFF
      FF00000000000000000000000000000000007B7B7B00FFFFFF00000000000000
      000000000000000000007B7B7B00FFFFFF00FFFF0000000000000000000000FF
      FF00FFFFFF0000FFFF00FFFFFF0000FFFF000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B007B7B7B000000
      0000000000000000000000000000FFFFFF007B7B7B00FFFFFF00000000000000
      0000000000000000000000000000FFFFFF000000000000000000000000007B7B
      7B007B7B7B007B7B7B00BDBDBD00BDBDBD0000000000BDBDBD00BDBDBD007B7B
      7B007B7B7B007B7B7B00000000000000000000000000000000007B7B7B00FFFF
      FF00000000000000000000000000000000007B7B7B00FFFFFF00FFFFFF000000
      000000000000000000007B7B7B00FFFFFF00FFFF00000000000000FFFF00FFFF
      FF0000FFFF00FFFFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B00FFFFFF000000
      000000000000000000007B7B7B007B7B7B007B7B7B0000000000FFFFFF000000
      000000000000000000007B7B7B007B7B7B00000000000000000000000000BDBD
      BD00BDBDBD00BDBDBD00BDBDBD00000000000000000000000000BDBDBD00BDBD
      BD00BDBDBD00BDBDBD00000000000000000000000000000000007B7B7B00FFFF
      FF000000000000000000000000007B7B7B007B7B7B007B7B7B00FFFFFF000000
      000000000000000000007B7B7B00FFFFFF00FFFF000000000000FFFFFF0000FF
      FF00FFFFFF0000FFFF00FFFFFF0000FFFF00FFFFFF0000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B00FFFFFF000000
      0000000000000000000000000000FFFFFF00FFFFFF007B7B7B00FFFFFF00FFFF
      FF00FFFFFF00000000007B7B7B007B7B7B000000000000000000000000007B7B
      7B007B7B7B007B7B7B007B7B7B000000000000000000000000007B7B7B007B7B
      7B007B7B7B007B7B7B00000000000000000000000000000000007B7B7B00FFFF
      FF000000000000000000000000007B7B7B007B7B7B007B7B7B00000000000000
      000000000000000000007B7B7B00FFFFFF00FFFF00000000000000FFFF00FFFF
      FF0000FFFF00FFFFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B00FFFFFF000000
      000000000000000000007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B0000000000FFFFFF0000000000FFFFFF00000000000000000000000000BDBD
      BD00BDBDBD00BDBDBD00BDBDBD00BDBDBD00BDBDBD00BDBDBD00BDBDBD00BDBD
      BD00BDBDBD00BDBDBD00000000000000000000000000000000007B7B7B000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF007B7B7B0000000000FFFF000000000000FFFFFF0000FF
      FF00FFFFFF0000FFFF00FFFFFF0000FFFF00FFFFFF0000FFFF00FFFFFF0000FF
      FF0000000000000000000000FF000000FF007B7B7B007B7B7B00FFFFFF000000
      000000000000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF007B7B7B00000000007B7B7B007B7B7B000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B000000000000000000FFFF00000000000000FFFF00FFFF
      FF00000000000000000000000000000000000000000000000000000000000000
      000000000000000000000000FF000000FF007B7B7B007B7B7B00FFFFFF00FFFF
      FF007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B0000000000000000007B7B7B007B7B7B000000000000000000000000000000
      000000000000BDBDBD000000000000000000000000000000000000000000BDBD
      BD00000000000000000000000000000000000000000000000000000000000000
      00007B7B7B00FFFFFF007B7B7B00FFFFFF0000000000000000007B7B7B00FFFF
      FF007B7B7B00FFFFFF00000000000000000000000000000000000000000000FF
      FF00FFFFFF0000FFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B007B7B7B000000
      0000FFFFFF00FFFFFF007B7B7B00000000000000000000000000000000000000
      0000000000000000000000000000FFFFFF000000000000000000000000000000
      000000000000BDBDBD000000000000000000000000000000000000000000BDBD
      BD00000000000000000000000000000000000000000000000000000000000000
      00007B7B7B00FFFFFF007B7B7B00FFFFFF0000000000000000007B7B7B00FFFF
      FF007B7B7B00FFFFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B007B7B7B007B7B7B0000000000000000000000000000000000000000000000
      000000000000000000007B7B7B007B7B7B000000000000000000000000000000
      000000000000BDBDBD000000000000000000000000000000000000000000BDBD
      BD00000000000000000000000000000000000000000000000000000000000000
      00007B7B7B00FFFFFF007B7B7B0000000000FFFFFF00FFFFFF007B7B7B000000
      00007B7B7B00FFFFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      000000000000000000007B7B7B007B7B7B000000000000000000000000000000
      00007B7B7B007B7B7B00BDBDBD00000000000000000000000000BDBDBD007B7B
      7B007B7B7B000000000000000000000000000000000000000000000000000000
      00007B7B7B0000000000FFFFFF007B7B7B007B7B7B007B7B7B00000000000000
      00007B7B7B000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000FFFFFF00FFFFFF000000000000000000000000000000
      00000000000000000000BDBDBD00BDBDBD00BDBDBD00BDBDBD00BDBDBD000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000007B7B7B0000000000FFFFFF00FFFFFF00FFFFFF00FFFFFF007B7B
      7B00000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000007B7B7B007B7B7B007B7B7B000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      000000000000000000007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000007B7B7B007B7B7B007B7B7B000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000000000000000000000FFFF0000000000000000000000
      000000000000000000000000000000FFFF000000000000000000FFFFFF00FFFF
      FF00000000000000000000000000000000007B7B7B00FFFFFF00000000000000
      00000000000000000000FFFFFF007B7B7B000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B00FFFFFF000000000000000000000000000000000000FFFF0000FFFF000000
      00007B7B7B007B7B7B007B7B7B0000FFFF0000FFFF007B7B7B007B7B7B007B7B
      7B007B7B7B0000FFFF0000FFFF0000000000000000007B7B7B007B7B7B00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF007B7B7B007B7B7B00FFFFFF00FFFFFF00FFFF
      FF00FFFFFF007B7B7B007B7B7B00000000000000000000000000000000000000
      0000FFFFFF007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF0000000000FFFFFF0000000000FFFFFF0000000000FFFFFF007B7B
      7B00FFFFFF00000000000000000000000000000000000000000000FFFF000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000FFFF00000000000000000000000000000000007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B0000000000000000000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD0000000000BDBDBD00000000007B7B7B000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF00000000007B7B7B0000000000000000000000000000000000000000007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      00007B7B7B00FFFFFF0000000000000000000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD00000000007B7B7B00000000007B7B7B000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF00000000007B7B7B0000000000000000000000000000000000000000007B7B
      7B00FFFFFF0000000000FFFFFF00FFFFFF0000000000FFFFFF00FFFFFF00FFFF
      FF007B7B7B00FFFFFF0000000000000000000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD0000000000BDBDBD00000000007B7B7B000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      0000FFFFFF000000000000000000FFFFFF00000000000000000000000000FFFF
      FF00000000007B7B7B0000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B007B7B7B00000000007B7B7B007B7B7B007B7B7B000000
      00007B7B7B00FFFFFF0000000000000000000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD00000000007B7B7B00000000007B7B7B000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF00000000007B7B7B00000000000000000000000000FFFFFF00FFFFFF007B7B
      7B00FFFFFF0000000000FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000FFFF
      FF007B7B7B00FFFFFF00FFFFFF00FFFFFF000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD0000000000BDBDBD00000000007B7B7B000000
      0000000000000000000000000000000000000000000000000000FFFFFF007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B00FFFFFF0000000000FFFFFF000000000000FFFF0000FFFF0000FFFF000000
      0000FFFFFF0000000000000000000000000000000000FFFFFF0000000000FFFF
      FF000000000000FFFF0000FFFF00000000007B7B7B007B7B7B007B7B7B007B7B
      7B00FFFFFF007B7B7B007B7B7B007B7B7B007B7B7B00000000007B7B7B000000
      00007B7B7B007B7B7B007B7B7B00FFFFFF000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD00000000007B7B7B00000000007B7B7B000000
      000000000000000000000000000000000000000000007B7B7B00000000007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B00FFFFFF007B7B7B0000000000000000000000000000FFFF0000FFFF000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF000000000000FFFF0000FFFF0000FFFF00000000007B7B7B007B7B7B007B7B
      7B00FFFFFF0000000000FFFFFF00FFFFFF0000000000FFFFFF00FFFFFF00FFFF
      FF007B7B7B007B7B7B007B7B7B007B7B7B000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD0000000000BDBDBD00000000007B7B7B000000
      00000000000000000000000000000000000000000000000000007B7B7B007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B007B7B7B000000000000000000000000000000000000000000000000000000
      0000FFFFFF000000000000000000FFFFFF000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B007B7B7B00000000007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B000000000000000000000000000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD00000000007B7B7B00000000007B7B7B000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000FFFFFF00FFFFFF000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF0000000000FFFFFF00FFFFFF007B7B7B00FFFFFF00000000007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      00007B7B7B00000000007B7B7B00000000007B7B7B00000000007B7B7B000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B7B00FFFFFF007B7B
      7B00FFFFFF00FFFFFF0000000000000000000000000000000000000000000000
      0000FFFFFF0000000000BDBDBD00FFFFFF0000000000FFFFFF000000000000FF
      FF00000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF007B7B7B007B7B7B00000000007B7B7B00FFFFFF007B7B7B007B7B
      7B00FFFFFF00FFFFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000000000000000000000000000000000007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B00FFFFFF0000000000000000000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000000000000000000000FF
      FF0000FFFF000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF00FFFFFF00FFFFFF00FFFFFF007B7B7B007B7B7B00000000007B7B
      7B007B7B7B00FFFFFF00FFFFFF0000000000000000000000000000000000FFFF
      FF00BDBDBD00BDBDBD00BDBDBD007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B000000000000000000000000000000000000000000000000007B7B7B00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF007B7B7B00FFFFFF000000000000000000000000000000000000FFFF000000
      0000000000000000000000000000000000000000000000000000000000000000
      000000FFFF0000FFFF00000000000000000000000000000000007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B00FFFFFF00000000000000
      00007B7B7B007B7B7B00FFFFFF00FFFFFF000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000000000000000000000000000000000007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B000000000000000000000000000000000000FFFF0000FFFF000000
      000000000000000000000000000000FFFF0000FFFF0000000000000000000000
      00000000000000FFFF0000FFFF0000000000000000007B7B7B007B7B7B000000
      00000000000000000000000000007B7B7B007B7B7B0000000000000000000000
      0000000000007B7B7B007B7B7B00000000000000000000000000000000000000
      000000000000000000007B7B7B007B7B7B007B7B7B0000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000007B7B7B00FFFFFF00FFFFFF00FFFFFF007B7B7B00FFFFFF000000
      00000000000000000000000000000000000000FFFF0000000000000000000000
      000000000000000000000000000000FFFF000000000000000000000000000000
      000000000000000000000000000000FFFF007B7B7B0000000000000000000000
      00000000000000000000000000007B7B7B000000000000000000000000000000
      00000000000000000000000000007B7B7B000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B00000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000000000000000000000FFFF0000000000000000000000
      000000000000000000000000000000FFFF000000000000000000FFFFFF00FFFF
      FF00000000000000000000000000000000007B7B7B00FFFFFF00000000000000
      00000000000000000000FFFFFF007B7B7B000000000000000000000000000000
      00000000000000000000000000000000000000000000000000007B7B7B000000
      00007B7B7B007B7B7B0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B000000000000FFFF0000FFFF000000
      00007B7B7B007B7B7B007B7B7B0000FFFF0000FFFF007B7B7B007B7B7B007B7B
      7B007B7B7B0000FFFF0000FFFF0000000000000000007B7B7B007B7B7B00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF007B7B7B007B7B7B00FFFFFF00FFFFFF00FFFF
      FF00FFFFFF007B7B7B007B7B7B00000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000BDBDBD000000
      0000BDBDBD00BDBDBD0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B00FFFFFF007B7B
      7B0000000000000000007B7B7B007B7B7B00000000000000000000FFFF000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000FFFF00000000000000000000000000000000007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000BDBDBD00BDBD
      BD00BDBDBD00BDBDBD0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF007B7B7B007B7B7B000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF00000000007B7B7B0000000000000000000000000000000000000000007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      00007B7B7B00FFFFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000000000000000000000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF00FFFFFF007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF00000000007B7B7B0000000000000000000000000000000000000000007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      00007B7B7B00FFFFFF0000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF00000000007B7B7B0000000000000000000000000000000000000000007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      00007B7B7B00FFFFFF00000000000000000000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF00000000007B7B7B00FFFFFF00000000000000
      0000000000000000000000000000000000007B7B7B00FFFFFF00000000000000
      00000000000000000000000000007B7B7B000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF00000000007B7B7B00000000000000000000000000FFFFFF00FFFFFF007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      00007B7B7B00FFFFFF00FFFFFF00FFFFFF0000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF00000000007B7B7B00FFFFFF0000000000FFFF
      FF00FFFFFF0000000000FFFFFF00FFFFFF007B7B7B00FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF007B7B7B0000FFFF0000FFFF0000FFFF000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF000000000000FFFF0000FFFF00000000007B7B7B007B7B7B007B7B7B007B7B
      7B00FFFFFF000000000000000000000000000000000000000000000000000000
      00007B7B7B007B7B7B007B7B7B00FFFFFF0000000000FFFFFF00000000000000
      0000FFFFFF000000000000000000BDBDBD0000000000FF000000FF000000FF00
      00000000FF00FF000000FF000000000000007B7B7B00FFFFFF007B7B7B007B7B
      7B00000000007B7B7B007B7B7B00000000007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B000000000000FFFF0000FFFF000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFF
      FF000000000000FFFF0000FFFF0000FFFF00000000007B7B7B007B7B7B007B7B
      7B00FFFFFF0000000000000000000000000000000000FFFFFF00FFFFFF00FFFF
      FF007B7B7B007B7B7B007B7B7B007B7B7B0000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000000000000000
      FF000000FF000000FF0000000000000000007B7B7B00FFFFFF0000000000FFFF
      FF00FFFFFF00FFFFFF00FFFFFF0000000000FFFFFF007B7B7B00FFFFFF007B7B
      7B007B7B7B007B7B7B00FFFFFF00000000000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF000000000000000000000000007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B0000000000000000000000000000000000FFFFFF00000000000000
      00000000000000000000FFFFFF0000000000FFFFFF00000000000000FF000000
      FF000000FF000000FF000000FF00000000007B7B7B00FFFFFF007B7B7B007B7B
      7B007B7B7B007B7B7B00000000007B7B7B00000000007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B00FFFFFF000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000FFFFFF00FFFFFF000000
      0000000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF000000000000000000000000007B7B7B00FFFFFF00000000007B7B
      7B00FFFFFF0000000000000000000000000000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF00FFFFFF00FFFFFF00FFFFFF00FFFFFF000000FF000000FF000000
      FF000000FF000000FF000000FF000000FF007B7B7B00FFFFFF0000000000FFFF
      FF00FFFFFF0000000000FFFFFF00FFFFFF00FFFFFF007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000FFFFFF000000000000FF
      FF00000000000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF000000000000000000000000007B7B7B00FFFFFF007B7B7B007B7B
      7B00FFFFFF00FFFFFF00000000000000000000000000FFFFFF00000000000000
      0000FFFFFF000000000000000000000000000000000000000000000000000000
      FF000000FF000000FF0000000000000000007B7B7B00FFFFFF007B7B7B007B7B
      7B00000000007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B00000000007B7B
      7B007B7B7B007B7B7B00FFFFFF00000000000000000000000000000000000000
      0000FFFFFF00FFFFFF00FFFFFF00FFFFFF0000000000000000000000000000FF
      FF0000FFFF000000000000000000000000000000000000000000000000007B7B
      7B00FFFFFF00FFFFFF00FFFFFF00FFFFFF007B7B7B007B7B7B00000000007B7B
      7B007B7B7B00FFFFFF00FFFFFF000000000000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF0000000000FFFFFF00FFFFFF000000000000000000000000000000
      FF000000FF000000FF0000000000000000007B7B7B00FFFFFF0000000000FFFF
      FF00FFFFFF007B7B7B00FFFFFF00000000007B7B7B0000000000000000007B7B
      7B007B7B7B007B7B7B00FFFFFF0000000000000000000000000000FFFF000000
      0000000000000000000000000000000000000000000000000000000000000000
      000000FFFF0000FFFF00000000000000000000000000000000007B7B7B007B7B
      7B007B7B7B007B7B7B007B7B7B007B7B7B007B7B7B00FFFFFF00000000000000
      00007B7B7B007B7B7B00FFFFFF00FFFFFF0000000000FFFFFF0000000000BDBD
      BD00FFFFFF0000000000FFFFFF000000000000000000000000007B7B7B000000
      FF000000FF000000FF0000000000000000007B7B7B00FFFFFF007B7B7B007B7B
      7B00000000007B7B7B00FFFFFF007B7B7B0000000000FFFFFF007B7B7B007B7B
      7B007B7B7B007B7B7B0000000000000000000000000000FFFF0000FFFF000000
      000000000000000000000000000000FFFF0000FFFF0000000000000000000000
      00000000000000FFFF0000FFFF0000000000000000007B7B7B007B7B7B000000
      00000000000000000000000000007B7B7B007B7B7B0000000000000000000000
      0000000000007B7B7B007B7B7B000000000000000000FFFFFF00FFFFFF00FFFF
      FF00FFFFFF000000000000000000000000000000FF000000FF000000FF000000
      FF000000FF000000000000000000000000007B7B7B00FFFFFF00FFFFFF00FFFF
      FF00FFFFFF007B7B7B007B7B7B00000000007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B0000000000000000000000000000FFFF0000000000000000000000
      000000000000000000000000000000FFFF000000000000000000000000000000
      000000000000000000000000000000FFFF007B7B7B0000000000000000000000
      00000000000000000000000000007B7B7B000000000000000000000000000000
      00000000000000000000000000007B7B7B000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000007B7B7B007B7B7B007B7B7B007B7B
      7B007B7B7B007B7B7B0000000000000000000000000000000000000000000000
      000000000000000000000000000000000000424D3E000000000000003E000000
      2800000040000000400000000100010000000000000200000000000000000000
      000000000000000000000000FFFFFF0000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      00000000000000000000000000000000FC1FF813FFFFFFFCF007F3A5FFF89078
      E003EF3A20F800B8C001CF3C007F1E3EC001CF1C007C1C5CC001CE1C003C1E04
      C001CE3C000F1C0AC001D00100041804E003E003000C000CF1C7F0C301FF11FE
      F1C7F0C3E3FCE3FCF1C7F113FFFCFFFCF007F437FFFFFFFCF80FFA0FFFF8FFF8
      FC1FFC1FFFF8FFF8FFFFFFFFFFFFFFFFFF7ECF3CE00FE00790018001E00FE547
      C003C003E00FE007E003E7F3E00FE007E003E483E00FE007E003E113E00FE007
      E0038420E00FC00500010050A00BA00380008480C007C007E007E107E00FE007
      E00FE427E00FE003E00FE103C007C003E027E021C007C003C073C030C007C007
      9E799E79F83FF81F7EFE7EFEF83FF83FFF7ECF3CFF00FF0090018001FF00FF0C
      C003C003FF00FF00E003E7F3FF008000E003E7F300000000E003E7F300003F3E
      E00387F000002400000107F0000009008000878000232101E007E70700010280
      E00FE72700002400E00FE70300230821E027E02100632161C073C03000C30883
      9E799E79010701077EFE7EFE03FF03FF00000000000000000000000000000000
      000000000000}
  end
end
