object MainForm: TMainForm
  Left = 1081
  Top = 601
  ActiveControl = ScrollBar
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = 'Synchronet User Editor'
  ClientHeight = 359
  ClientWidth = 447
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  Menu = MainMenu
  OldCreateOrder = False
  Position = poDefaultPosOnly
  OnClose = FormClose
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object Panel: TPanel
    Left = 0
    Top = 0
    Width = 447
    Height = 359
    Align = alClient
    BevelOuter = bvNone
    TabOrder = 0
    object PageControl: TPageControl
      Left = 0
      Top = 81
      Width = 447
      Height = 278
      ActivePage = StatsTabSheet
      Align = alClient
      TabIndex = 2
      TabOrder = 0
      object PersonalTabSheet: TTabSheet
        Caption = 'Personal'
        object Label2: TLabel
          Left = 8
          Top = 8
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Real Name'
        end
        object Label1: TLabel
          Left = 8
          Top = 32
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Host Name'
        end
        object NetmailAddress: TLabel
          Left = 8
          Top = 56
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'NetMail'
        end
        object Label7: TLabel
          Left = 8
          Top = 104
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Note'
        end
        object HandleLabel: TLabel
          Left = 296
          Top = 56
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Handle'
        end
        object Label5: TLabel
          Left = 296
          Top = 104
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Password'
        end
        object Label6: TLabel
          Left = 296
          Top = 80
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Birthdate'
        end
        object Label8: TLabel
          Left = 296
          Top = 32
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Connection'
        end
        object Label16: TLabel
          Left = 296
          Top = 8
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Gender'
        end
        object Label18: TLabel
          Left = 8
          Top = 80
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Phone'
        end
        object Label24: TLabel
          Left = 8
          Top = 128
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Comment'
        end
        object NameEdit: TEdit
          Left = 80
          Top = 8
          Width = 201
          Height = 21
          TabOrder = 0
          OnChange = EditChange
        end
        object ComputerEdit: TEdit
          Left = 80
          Top = 32
          Width = 201
          Height = 21
          TabOrder = 1
          OnChange = EditChange
        end
        object NetmailEdit: TEdit
          Left = 80
          Top = 56
          Width = 201
          Height = 21
          TabOrder = 2
          OnChange = EditChange
        end
        object NoteEdit: TEdit
          Left = 80
          Top = 104
          Width = 201
          Height = 21
          TabOrder = 4
          OnChange = EditChange
        end
        object HandleEdit: TEdit
          Left = 368
          Top = 56
          Width = 57
          Height = 21
          TabOrder = 8
          OnChange = EditChange
        end
        object PasswordEdit: TEdit
          Left = 368
          Top = 104
          Width = 57
          Height = 21
          CharCase = ecUpperCase
          PasswordChar = '*'
          TabOrder = 10
          OnChange = EditChange
        end
        object BirthdateEdit: TEdit
          Left = 368
          Top = 80
          Width = 57
          Height = 21
          TabOrder = 9
          OnChange = EditChange
        end
        object ModemEdit: TEdit
          Left = 368
          Top = 32
          Width = 57
          Height = 21
          TabOrder = 7
          OnChange = EditChange
        end
        object SexEdit: TEdit
          Left = 368
          Top = 8
          Width = 17
          Height = 21
          TabOrder = 6
          OnChange = EditChange
        end
        object PhoneEdit: TEdit
          Left = 80
          Top = 80
          Width = 201
          Height = 21
          TabOrder = 3
          OnChange = EditChange
        end
        object CommentEdit: TEdit
          Left = 80
          Top = 128
          Width = 345
          Height = 21
          TabOrder = 5
          OnChange = EditChange
        end
        object AddressGroupBox: TGroupBox
          Left = 72
          Top = 160
          Width = 297
          Height = 73
          Caption = 'Address'
          TabOrder = 11
          DesignSize = (
            297
            73)
          object AddressEdit: TEdit
            Left = 8
            Top = 20
            Width = 201
            Height = 21
            Anchors = [akLeft, akTop, akRight]
            TabOrder = 0
            OnChange = EditChange
          end
          object LocationEdit: TEdit
            Left = 8
            Top = 44
            Width = 201
            Height = 21
            Anchors = [akLeft, akTop, akRight]
            TabOrder = 1
            OnChange = EditChange
          end
          object ZipCodeEdit: TEdit
            Left = 216
            Top = 44
            Width = 73
            Height = 21
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
          Left = 8
          Top = 8
          Width = 41
          Height = 21
          AutoSize = False
          Caption = 'Level'
        end
        object Label11: TLabel
          Left = 112
          Top = 8
          Width = 57
          Height = 21
          AutoSize = False
          Caption = 'Expiration'
        end
        object Label25: TLabel
          Left = 8
          Top = 184
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Exemptions'
        end
        object Label26: TLabel
          Left = 8
          Top = 208
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Restrictions'
        end
        object Label27: TLabel
          Left = 264
          Top = 8
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Credits'
        end
        object Label28: TLabel
          Left = 264
          Top = 104
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Minutes'
        end
        object Label29: TLabel
          Left = 264
          Top = 56
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Free Credits'
        end
        object LevelEdit: TEdit
          Left = 48
          Top = 8
          Width = 25
          Height = 21
          MaxLength = 2
          TabOrder = 0
          OnChange = EditChange
        end
        object ExpireEdit: TEdit
          Left = 176
          Top = 8
          Width = 57
          Height = 21
          TabOrder = 1
          OnChange = EditChange
        end
        object ExemptionsEdit: TEdit
          Left = 80
          Top = 184
          Width = 201
          Height = 21
          CharCase = ecUpperCase
          TabOrder = 3
          OnChange = EditChange
        end
        object RestrictionsEdit: TEdit
          Left = 80
          Top = 208
          Width = 201
          Height = 21
          CharCase = ecUpperCase
          TabOrder = 4
          OnChange = EditChange
        end
        object CreditsEdit: TEdit
          Left = 336
          Top = 8
          Width = 89
          Height = 21
          CharCase = ecUpperCase
          MaxLength = 26
          TabOrder = 5
          OnChange = EditChange
        end
        object MinutesEdit: TEdit
          Left = 336
          Top = 104
          Width = 89
          Height = 21
          CharCase = ecUpperCase
          MaxLength = 26
          TabOrder = 7
          OnChange = EditChange
        end
        object FreeCreditsEdit: TEdit
          Left = 336
          Top = 56
          Width = 89
          Height = 21
          CharCase = ecUpperCase
          MaxLength = 26
          TabOrder = 6
          OnChange = EditChange
        end
        object FlagSetGroupBox: TGroupBox
          Left = 8
          Top = 40
          Width = 233
          Height = 129
          Caption = 'Flag Sets'
          TabOrder = 2
          object Label12: TLabel
            Left = 8
            Top = 24
            Width = 17
            Height = 21
            AutoSize = False
            Caption = '1'
          end
          object Label13: TLabel
            Left = 8
            Top = 48
            Width = 17
            Height = 21
            AutoSize = False
            Caption = '2'
          end
          object Label14: TLabel
            Left = 8
            Top = 72
            Width = 17
            Height = 21
            AutoSize = False
            Caption = '3'
          end
          object Label15: TLabel
            Left = 8
            Top = 96
            Width = 17
            Height = 21
            AutoSize = False
            Caption = '4'
          end
          object Flags1Edit: TEdit
            Left = 24
            Top = 24
            Width = 201
            Height = 21
            CharCase = ecUpperCase
            TabOrder = 0
            OnChange = EditChange
          end
          object Flags2Edit: TEdit
            Left = 24
            Top = 48
            Width = 201
            Height = 21
            CharCase = ecUpperCase
            TabOrder = 1
            OnChange = EditChange
          end
          object Flags3Edit: TEdit
            Left = 24
            Top = 72
            Width = 201
            Height = 21
            CharCase = ecUpperCase
            TabOrder = 2
            OnChange = EditChange
          end
          object Flags4Edit: TEdit
            Left = 24
            Top = 96
            Width = 201
            Height = 21
            CharCase = ecUpperCase
            TabOrder = 3
            OnChange = EditChange
          end
        end
        object CreditsStaticText: TStaticText
          Left = 336
          Top = 32
          Width = 89
          Height = 21
          AutoSize = False
          BorderStyle = sbsSunken
          TabOrder = 8
        end
        object FreeCreditsStaticText: TStaticText
          Left = 336
          Top = 80
          Width = 89
          Height = 21
          AutoSize = False
          BorderStyle = sbsSunken
          TabOrder = 9
        end
      end
      object StatsTabSheet: TTabSheet
        Caption = 'Statistics'
        ImageIndex = 2
        object TimeOnGroupBox: TGroupBox
          Left = 152
          Top = 120
          Width = 137
          Height = 121
          Caption = 'Time On'
          TabOrder = 4
          DesignSize = (
            137
            121)
          object Label22: TLabel
            Left = 8
            Top = 20
            Width = 60
            Height = 21
            AutoSize = False
            Caption = 'Total'
          end
          object Label23: TLabel
            Left = 8
            Top = 44
            Width = 60
            Height = 21
            AutoSize = False
            Caption = 'Today'
          end
          object Label30: TLabel
            Left = 8
            Top = 92
            Width = 60
            Height = 21
            AutoSize = False
            Caption = 'Extra'
          end
          object Label31: TLabel
            Left = 8
            Top = 68
            Width = 60
            Height = 21
            AutoSize = False
            Caption = 'Last Call'
          end
          object TimeOnEdit: TEdit
            Left = 72
            Top = 20
            Width = 57
            Height = 21
            Anchors = [akTop, akRight]
            TabOrder = 0
            OnChange = EditChange
          end
          object TimeOnTodayEdit: TEdit
            Left = 72
            Top = 44
            Width = 57
            Height = 21
            Anchors = [akTop, akRight]
            TabOrder = 1
            OnChange = EditChange
          end
          object ExtraTimeEdit: TEdit
            Left = 72
            Top = 92
            Width = 57
            Height = 21
            Anchors = [akTop, akRight]
            TabOrder = 3
            OnChange = EditChange
          end
          object LastCallTimeEdit: TEdit
            Left = 72
            Top = 68
            Width = 57
            Height = 21
            Anchors = [akTop, akRight]
            TabOrder = 2
            OnChange = EditChange
          end
        end
        object LogonsGroupBoxx: TGroupBox
          Left = 8
          Top = 88
          Width = 137
          Height = 73
          Caption = 'Logons'
          TabOrder = 1
          object Label20: TLabel
            Left = 8
            Top = 20
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Total'
          end
          object Label21: TLabel
            Left = 8
            Top = 44
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Today'
          end
          object LogonsEdit: TEdit
            Left = 72
            Top = 20
            Width = 57
            Height = 21
            TabOrder = 0
            OnChange = EditChange
          end
          object LogonsTodayEdit: TEdit
            Left = 72
            Top = 44
            Width = 57
            Height = 21
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object DatesGroupBox: TGroupBox
          Left = 8
          Top = 8
          Width = 137
          Height = 73
          Caption = 'Dates'
          TabOrder = 0
          object Label17: TLabel
            Left = 8
            Top = 20
            Width = 60
            Height = 21
            AutoSize = False
            Caption = 'First on'
          end
          object Label19: TLabel
            Left = 8
            Top = 44
            Width = 60
            Height = 21
            AutoSize = False
            Caption = 'Last on'
          end
          object FirstOnEdit: TEdit
            Left = 72
            Top = 20
            Width = 57
            Height = 21
            TabOrder = 0
            OnChange = EditChange
          end
          object LastOnEdit: TEdit
            Left = 72
            Top = 44
            Width = 57
            Height = 21
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object PostsGroupBox: TGroupBox
          Left = 8
          Top = 168
          Width = 137
          Height = 73
          Caption = 'Posts'
          TabOrder = 2
          object Label32: TLabel
            Left = 8
            Top = 20
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Total'
          end
          object Label33: TLabel
            Left = 8
            Top = 44
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Today'
          end
          object PostsTotalEdit: TEdit
            Left = 72
            Top = 20
            Width = 57
            Height = 21
            TabOrder = 0
            OnChange = EditChange
          end
          object PostsTodayEdit: TEdit
            Left = 72
            Top = 44
            Width = 57
            Height = 21
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object EmailGroupBox: TGroupBox
          Left = 296
          Top = 144
          Width = 137
          Height = 97
          Caption = 'E-mail'
          TabOrder = 6
          object Label34: TLabel
            Left = 8
            Top = 20
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Total'
          end
          object Label35: TLabel
            Left = 8
            Top = 44
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Today'
          end
          object Label36: TLabel
            Left = 8
            Top = 68
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'To Sysop'
          end
          object EmailTotalEdit: TEdit
            Left = 72
            Top = 20
            Width = 57
            Height = 21
            TabOrder = 0
            OnChange = EditChange
          end
          object EmailTodayEdit: TEdit
            Left = 72
            Top = 44
            Width = 57
            Height = 21
            TabOrder = 1
            OnChange = EditChange
          end
          object FeedbackEdit: TEdit
            Left = 72
            Top = 68
            Width = 57
            Height = 21
            TabOrder = 2
            OnChange = EditChange
          end
        end
        object UploadsGroupBox: TGroupBox
          Left = 152
          Top = 8
          Width = 137
          Height = 97
          Caption = 'Uploads'
          TabOrder = 3
          object Label37: TLabel
            Left = 8
            Top = 20
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Total'
          end
          object Label38: TLabel
            Left = 8
            Top = 44
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Bytes'
          end
          object UploadedFilesEdit: TEdit
            Left = 72
            Top = 20
            Width = 57
            Height = 21
            TabOrder = 0
            OnChange = EditChange
          end
          object UploadedBytesEdit: TEdit
            Left = 72
            Top = 44
            Width = 57
            Height = 21
            TabOrder = 1
            OnChange = EditChange
          end
          object UploadedBytesStaticText: TStaticText
            Left = 72
            Top = 68
            Width = 58
            Height = 21
            AutoSize = False
            BorderStyle = sbsSunken
            TabOrder = 2
          end
        end
        object DownloadsGroupBox: TGroupBox
          Left = 296
          Top = 8
          Width = 137
          Height = 121
          Caption = 'Downloads'
          TabOrder = 5
          object Label39: TLabel
            Left = 8
            Top = 20
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Total'
          end
          object Label40: TLabel
            Left = 8
            Top = 44
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Bytes'
          end
          object Label42: TLabel
            Left = 8
            Top = 92
            Width = 57
            Height = 21
            AutoSize = False
            Caption = 'Leech'
          end
          object DownloadedFilesEdit: TEdit
            Left = 72
            Top = 20
            Width = 57
            Height = 21
            TabOrder = 0
            OnChange = EditChange
          end
          object DownloadedBytesEdit: TEdit
            Left = 72
            Top = 44
            Width = 57
            Height = 21
            TabOrder = 1
            OnChange = EditChange
          end
          object LeechEdit: TEdit
            Left = 72
            Top = 92
            Width = 57
            Height = 21
            TabOrder = 2
            OnChange = EditChange
          end
          object DownloadedBytesStaticText: TStaticText
            Left = 72
            Top = 68
            Width = 58
            Height = 21
            AutoSize = False
            BorderStyle = sbsSunken
            TabOrder = 3
          end
        end
      end
      object SettingsTabSheet: TTabSheet
        Caption = 'Settings'
        ImageIndex = 3
        object TerminalGroupBox: TGroupBox
          Left = 8
          Top = 8
          Width = 209
          Height = 153
          Caption = 'Terminal'
          TabOrder = 0
          DesignSize = (
            209
            153)
          object Label3: TLabel
            Left = 136
            Top = 120
            Width = 40
            Height = 21
            AutoSize = False
            Caption = 'Rows'
          end
          object RowsEdit: TEdit
            Left = 176
            Top = 120
            Width = 25
            Height = 21
            MaxLength = 2
            TabOrder = 0
            OnChange = EditChange
          end
          object TerminalCheckListBox: TCheckListBox
            Left = 8
            Top = 24
            Width = 193
            Height = 89
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 13
            Items.Strings = (
              'Auto-Detect'
              'Extended ASCII'
              'ANSI'
              'Color'
              'RIP'
              'Pause'
              'Hot Keys'
              'Spinning Cursor')
            TabOrder = 1
          end
        end
        object LogonGroupBox: TGroupBox
          Left = 224
          Top = 8
          Width = 209
          Height = 113
          Caption = 'Logon'
          TabOrder = 2
          DesignSize = (
            209
            113)
          object LogonCheckListBox: TCheckListBox
            Left = 8
            Top = 24
            Width = 193
            Height = 73
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 13
            Items.Strings = (
              'Ask for New Message Scan'
              'Ask for Your Message Scan'
              'Remember Current Sub-board'
              'Quiet Mode (Q exempt)'
              'Auto-Logon via IP (V exempt)')
            TabOrder = 0
          end
        end
        object ChatGroupBox: TGroupBox
          Left = 224
          Top = 128
          Width = 209
          Height = 113
          Caption = 'Chat'
          TabOrder = 3
          DesignSize = (
            209
            113)
          object ChatCheckListBox: TCheckListBox
            Left = 8
            Top = 24
            Width = 193
            Height = 73
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 13
            Items.Strings = (
              'Multinode Chat Echo'
              'Multinode Chat Actions'
              'Available for Internode Chat'
              'Multinode Activity Alerts'
              'Split-screen Private Chat')
            TabOrder = 0
          end
        end
        object CommandShellGroupBox: TGroupBox
          Left = 8
          Top = 168
          Width = 209
          Height = 73
          Caption = 'Command Shell'
          TabOrder = 1
          object ShellEdit: TEdit
            Left = 8
            Top = 24
            Width = 73
            Height = 21
            CharCase = ecUpperCase
            TabOrder = 0
            OnChange = EditChange
          end
          object ExpertCheckBox: TCheckBox
            Left = 8
            Top = 48
            Width = 161
            Height = 17
            Caption = 'Expert Menu Mode'
            TabOrder = 1
            OnClick = EditChange
          end
        end
      end
      object MsgFileSettingsTabSheet: TTabSheet
        Caption = 'Msg/File Settings'
        ImageIndex = 4
        object MessageGroupBox: TGroupBox
          Left = 8
          Top = 8
          Width = 209
          Height = 97
          Caption = 'Message'
          TabOrder = 0
          DesignSize = (
            209
            97)
          object Label41: TLabel
            Left = 48
            Top = 64
            Width = 81
            Height = 21
            AutoSize = False
            Caption = 'External Editor'
          end
          object MessageCheckListBox: TCheckListBox
            Left = 8
            Top = 24
            Width = 193
            Height = 33
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 13
            Items.Strings = (
              'Forward Email to NetMail'
              'Clear Screen Between Messages')
            TabOrder = 0
          end
          object EditorEdit: TEdit
            Left = 128
            Top = 64
            Width = 73
            Height = 21
            Anchors = [akLeft, akTop, akRight]
            CharCase = ecUpperCase
            TabOrder = 1
            OnChange = EditChange
          end
        end
        object FileGroupBox: TGroupBox
          Left = 224
          Top = 8
          Width = 209
          Height = 153
          Caption = 'File'
          TabOrder = 2
          DesignSize = (
            209
            153)
          object Label4: TLabel
            Left = 48
            Top = 120
            Width = 121
            Height = 21
            Anchors = [akLeft, akTop, akBottom]
            AutoSize = False
            Caption = 'Temp/QWK File Type'
          end
          object Label9: TLabel
            Left = 48
            Top = 88
            Width = 137
            Height = 21
            Anchors = [akLeft, akTop, akBottom]
            AutoSize = False
            Caption = 'Default Download Protocol'
          end
          object TempFileExtEdit: TEdit
            Left = 168
            Top = 120
            Width = 33
            Height = 21
            Anchors = [akLeft, akTop, akBottom]
            CharCase = ecUpperCase
            MaxLength = 3
            TabOrder = 0
            OnChange = EditChange
          end
          object ProtocolEdit: TEdit
            Left = 184
            Top = 88
            Width = 17
            Height = 21
            Anchors = [akLeft, akTop, akBottom]
            CharCase = ecUpperCase
            MaxLength = 1
            TabOrder = 1
            OnChange = EditChange
          end
          object FileCheckListBox: TCheckListBox
            Left = 8
            Top = 24
            Width = 193
            Height = 57
            OnClickCheck = EditChange
            Anchors = [akLeft, akRight, akBottom]
            ItemHeight = 13
            Items.Strings = (
              'Auto-New Scan'
              'Extended Descriptions'
              'Batch Flagging'
              'Auto-Hangup After Transfer')
            TabOrder = 2
          end
        end
        object QwkGroupBox: TGroupBox
          Left = 8
          Top = 112
          Width = 209
          Height = 129
          Caption = 'QWK Message Packet'
          TabOrder = 1
          DesignSize = (
            209
            129)
          object QWKCheckListBox: TCheckListBox
            Left = 8
            Top = 24
            Width = 193
            Height = 89
            OnClickCheck = EditChange
            Anchors = [akLeft, akTop, akRight]
            ItemHeight = 13
            Items.Strings = (
			  'Include QWKE Extensions'
              'Strip Ctrl-A codes'
              'Expand Ctrl-A Codes to ANSI'
			  'Include UTF-8 Characters'
              'Include New Files List'
              'Include File Attachments'
              'Include Messages From Self'
              'Include Unread Email'
              'Include ALL Email'
              'Delete Downloaded E-mail'
              'Include Index Files'
              'Include Control Files'
              'Include Time Zone Kludges'
              'Include Via/Routing Kludges'
			  'Include Message-ID Kludges'
			  'Include HEADERS.DAT File'
			  'Include VOTING.DAT File')
            TabOrder = 0
          end
        end
      end
      object ExtendedCommentTabSheet: TTabSheet
        Caption = 'Extended Comment'
        ImageIndex = 5
        object Memo: TMemo
          Left = 8
          Top = 8
          Width = 425
          Height = 233
          ScrollBars = ssBoth
          TabOrder = 0
          OnChange = EditChange
        end
      end
    end
    object TopPanel: TPanel
      Left = 0
      Top = 41
      Width = 447
      Height = 40
      Align = alTop
      BevelOuter = bvNone
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      object NumberLabel: TLabel
        Left = 288
        Top = 8
        Width = 49
        Height = 21
        AutoSize = False
        Caption = 'Number'
      end
      object AliasLabel: TLabel
        Left = 8
        Top = 8
        Width = 33
        Height = 21
        AutoSize = False
        Caption = 'Alias'
      end
      object NumberEdit: TEdit
        Left = 336
        Top = 8
        Width = 41
        Height = 21
        TabOrder = 1
        OnKeyPress = NumberEditKeyPress
      end
      object TotalStaticText: TStaticText
        Left = 384
        Top = 8
        Width = 57
        Height = 21
        AutoSize = False
        BorderStyle = sbsSunken
        TabOrder = 2
      end
      object AliasEdit: TEdit
        Left = 40
        Top = 8
        Width = 161
        Height = 21
        TabOrder = 0
        OnChange = EditChange
      end
    end
    object ToolBar: TToolBar
      Left = 0
      Top = 0
      Width = 447
      Height = 25
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
        Height = 22
        TabOrder = 0
        OnKeyPress = FindEditKeyPress
      end
      object FindButton: TButton
        Left = 247
        Top = 0
        Width = 42
        Height = 22
        Caption = 'Find'
        TabOrder = 2
        OnClick = FindButtonClick
      end
      object FindNextButton: TButton
        Left = 289
        Top = 0
        Width = 42
        Height = 22
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
        Height = 22
        Color = clMenu
        ReadOnly = True
        TabOrder = 3
      end
    end
    object ScrollBar: TScrollBar
      Left = 0
      Top = 25
      Width = 447
      Height = 16
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
      0000000000003600000028000000400000004000000001001000000000000020
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
      00000000000000000000000000000000000000000000000000000000FF7FEF3D
      EF3DEF3DEF3DEF3D0000FF7FFF7F000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000FF7FFF7F000000000000000000000000F75E
      F75EF75EF75EF75E000000000000000000000000000000000000EF3DEF3D0000
      00000000FF7F0000EF3DEF3D0000FF7F00000000000000000000000000000000
      0000000000000000000000000000000000000000FF7FFF7F0000FF7FFF7FFF7F
      FF7FFF7F0000000000000000EF3DEF3DEF3D0000000000000000EF3DEF3DF75E
      EF3D0000EF3DF75EEF3DEF3D000000000000000000000000EF3D000000000000
      0000EF3DFF7F000000000000EF3D0000FF7F0000000000000000000000000000
      000000000000000000000000000000000000EF3DEF3DFF7FEF3DEF3DEF3DEF3D
      EF3D0000FF7F000000000000EF3DEF3DEF3D000000000000F75EF75EF75EF75E
      EF3D0000EF3DF75EF75EF75EF75E0000000000000000EF3DFF7F000000000000
      0000EF3DFF7F0000000000000000EF3DFF7FFF0300000000E07FFF7FE07FFF7F
      E07F00000000000000000000000000000000EF3DEF3DEF3D0000000000000000
      FF7FEF3DFF7F00000000000000000000FF7F000000000000EF3DEF3DEF3DF75E
      F75E0000F75EF75EEF3DEF3DEF3D0000000000000000EF3DFF7F000000000000
      0000EF3DFF7FFF7F000000000000EF3DFF7FFF030000E07FFF7FE07FFF7F0000
      000000000000000000000000000000000000EF3DEF3DFF7F000000000000EF3D
      EF3DEF3D0000FF7F000000000000EF3DEF3D000000000000F75EF75EF75EF75E
      000000000000F75EF75EF75EF75E0000000000000000EF3DFF7F000000000000
      EF3DEF3DEF3DFF7F000000000000EF3DFF7FFF030000FF7FE07FFF7FE07FFF7F
      E07FFF7F0000000000000000000000000000EF3DEF3DFF7F0000000000000000
      FF7FFF7FEF3DFF7FFF7FFF7F0000EF3DEF3D000000000000EF3DEF3DEF3DEF3D
      000000000000EF3DEF3DEF3DEF3D0000000000000000EF3DFF7F000000000000
      EF3DEF3DEF3D0000000000000000EF3DFF7FFF030000E07FFF7FE07FFF7F0000
      000000000000000000000000000000000000EF3DEF3DFF7F000000000000EF3D
      EF3DEF3DEF3DEF3DEF3D0000FF7F0000FF7F000000000000F75EF75EF75EF75E
      F75EF75EF75EF75EF75EF75EF75E0000000000000000EF3D0000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7FFF7FFF7FEF3D0000FF030000FF7FE07FFF7FE07FFF7F
      E07FFF7FE07FFF7FE07F00000000007C007CEF3DEF3DFF7F00000000FF7FFF7F
      FF7FFF7FFF7FFF7FFF7FEF3D0000EF3DEF3D0000000000000000000000000000
      000000000000000000000000000000000000000000000000EF3DEF3DEF3DEF3D
      EF3DEF3DEF3DEF3DEF3DEF3DEF3D00000000FF030000E07FFF7F000000000000
      0000000000000000000000000000007C007CEF3DEF3DFF7FFF7FEF3DEF3DEF3D
      EF3DEF3DEF3DEF3DEF3D00000000EF3DEF3D00000000000000000000F75E0000
      0000000000000000F75E00000000000000000000000000000000EF3DFF7FEF3D
      FF7F00000000EF3DFF7FEF3DFF7F00000000000000000000E07FFF7FE07F0000
      000000000000000000000000000000000000EF3DEF3DEF3D0000FF7FFF7FEF3D
      00000000000000000000000000000000FF7F00000000000000000000F75E0000
      0000000000000000F75E00000000000000000000000000000000EF3DFF7FEF3D
      FF7F00000000EF3DFF7FEF3DFF7F000000000000000000000000000000000000
      000000000000000000000000000000000000000000000000EF3DEF3DEF3D0000
      0000000000000000000000000000EF3DEF3D00000000000000000000F75E0000
      0000000000000000F75E00000000000000000000000000000000EF3DFF7FEF3D
      0000FF7FFF7FEF3D0000EF3DFF7F000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000EF3DEF3D0000000000000000EF3DEF3DF75E
      000000000000F75EEF3DEF3D0000000000000000000000000000EF3D0000FF7F
      EF3DEF3DEF3D00000000EF3D0000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000FF7FFF7F000000000000000000000000F75E
      F75EF75EF75EF75E0000000000000000000000000000000000000000EF3D0000
      FF7FFF7FFF7FFF7FEF3D00000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      000000000000000000000000EF3DEF3DEF3D0000000000000000000000000000
      000000000000000000000000000000000000000000000000000000000000EF3D
      EF3DEF3DEF3DEF3D000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      000000000000000000000000EF3DEF3DEF3D0000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000000000000000000000000000000000000000000000000000000000000000
      0000E07F000000000000000000000000E07F00000000FF7FFF7F000000000000
      0000EF3DFF7F0000000000000000FF7FEF3D0000000000000000000000000000
      000000000000000000000000000000000000000000000000EF3DEF3DEF3DEF3D
      EF3DEF3DEF3DEF3DEF3DFF7F0000000000000000E07FE07F0000EF3DEF3DEF3D
      E07FE07FEF3DEF3DEF3DEF3DE07FE07F00000000EF3DEF3DFF7FFF7FFF7FFF7F
      EF3DEF3DFF7FFF7FFF7FFF7FEF3DEF3D00000000000000000000FF7FEF3DEF3D
      EF3DEF3DEF3DEF3D00000000000000000000000000000000EF3DFF7F0000FF7F
      0000FF7F0000FF7FEF3DFF7F00000000000000000000E07F0000000000000000
      000000000000000000000000E07F0000000000000000EF3DEF3DEF3DEF3DEF3D
      EF3DEF3DEF3DEF3DEF3DEF3DEF3D000000000000000000000000FF7F0000F75E
      0000F75E0000EF3D00000000000000000000000000000000EF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DFF7F0000000000000000000000000000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000EF3D00000000000000000000EF3DFF7F00000000
      00000000000000000000EF3DFF7F000000000000000000000000FF7F0000F75E
      0000EF3D0000EF3D00000000000000000000000000000000EF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DFF7F0000000000000000000000000000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000EF3D00000000000000000000EF3DFF7F0000FF7F
      FF7F0000FF7FFF7FFF7FEF3DFF7F000000000000000000000000FF7F0000F75E
      0000F75E0000EF3D00000000000000000000000000000000EF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DFF7F0000000000000000000000000000FF7F00000000
      FF7F000000000000FF7F0000EF3D00000000000000000000EF3DFF7FEF3DEF3D
      0000EF3DEF3DEF3D0000EF3DFF7F000000000000000000000000FF7F0000F75E
      0000EF3D0000EF3D00000000000000000000000000000000EF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DFF7F0000000000000000000000000000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000EF3D000000000000FF7FFF7FEF3DFF7F0000FF7F
      FF7FFF7FFF7F0000FF7FEF3DFF7FFF7FFF7F0000000000000000FF7F0000F75E
      0000F75E0000EF3D0000000000000000000000000000FF7FEF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DFF7F0000FF7F0000E07FE07FE07F0000FF7F00000000
      00000000FF7F0000FF7F0000E07FE07F0000EF3DEF3DEF3DEF3DFF7FEF3DEF3D
      EF3DEF3D0000EF3D0000EF3DEF3DEF3DFF7F0000000000000000FF7F0000F75E
      0000EF3D0000EF3D000000000000000000000000EF3D0000EF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DFF7FEF3D000000000000E07FE07F0000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000E07FE07FE07F0000EF3DEF3DEF3DFF7F0000FF7F
      FF7F0000FF7FFF7FFF7FEF3DEF3DEF3DEF3D0000000000000000FF7F0000F75E
      0000F75E0000EF3D0000000000000000000000000000EF3DEF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DEF3D0000000000000000000000000000FF7F00000000
      FF7F00000000000000000000000000000000000000000000EF3DFF7FEF3DEF3D
      0000EF3DEF3DEF3DEF3DEF3D0000000000000000000000000000FF7F0000F75E
      0000EF3D0000EF3D00000000000000000000000000000000EF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DFF7F0000000000000000000000000000FF7FFF7FFF7F
      FF7F0000FF7FFF7F00000000000000000000000000000000EF3DFF7F0000FF7F
      FF7FEF3DFF7F0000EF3DFF7F0000000000000000000000000000EF3D0000EF3D
      0000EF3D0000EF3D00000000000000000000000000000000EF3DFF7FEF3DFF7F
      EF3DFF7FEF3DFF7FEF3DFF7FFF7F000000000000000000000000FF7F0000F75E
      FF7F0000FF7F0000E07F0000000000000000000000000000EF3DFF7FEF3DEF3D
      0000EF3DFF7FEF3DEF3DFF7FFF7F000000000000000000000000000000000000
      00000000000000000000000000000000000000000000EF3DEF3DEF3DEF3DEF3D
      EF3DEF3DEF3DEF3DEF3DEF3DFF7F000000000000000000000000FF7FFF7FFF7F
      FF7F000000000000E07FE07F000000000000000000000000EF3DFF7FFF7FFF7F
      FF7FEF3DEF3D0000EF3DEF3DFF7FFF7F0000000000000000FF7FF75EF75EF75E
      EF3DEF3DEF3DEF3DEF3D000000000000000000000000EF3DFF7FFF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7FEF3DFF7F0000000000000000E07F0000000000000000
      00000000000000000000E07FE07F0000000000000000EF3DEF3DEF3DEF3DEF3D
      EF3DEF3DFF7F00000000EF3DEF3DFF7FFF7F0000000000000000000000000000
      00000000000000000000000000000000000000000000EF3DEF3DEF3DEF3DEF3D
      EF3DEF3DEF3DEF3DEF3DEF3D0000000000000000E07FE07F0000000000000000
      E07FE07F0000000000000000E07FE07F00000000EF3DEF3D0000000000000000
      EF3DEF3D0000000000000000EF3DEF3D0000000000000000000000000000EF3D
      EF3DEF3D000000000000000000000000000000000000000000000000EF3DFF7F
      FF7FFF7FEF3DFF7F00000000000000000000E07F000000000000000000000000
      E07F0000000000000000000000000000E07FEF3D000000000000000000000000
      EF3D0000000000000000000000000000EF3D0000000000000000000000000000
      00000000000000000000000000000000000000000000000000000000EF3DEF3D
      EF3DEF3DEF3D0000000000000000000000000000000000000000000000000000
      0000E07F000000000000000000000000E07F00000000FF7FFF7F000000000000
      0000EF3DFF7F0000000000000000FF7FEF3D0000000000000000000000000000
      000000000000EF3D0000EF3DEF3D000000000000000000000000000000000000
      0000EF3DEF3DEF3DEF3DEF3DEF3DEF3DEF3D0000E07FE07F0000EF3DEF3DEF3D
      E07FE07FEF3DEF3DEF3DEF3DE07FE07F00000000EF3DEF3DFF7FFF7FFF7FFF7F
      EF3DEF3DFF7FFF7FFF7FFF7FEF3DEF3D00000000000000000000000000000000
      000000000000F75E0000F75EF75E000000000000000000000000000000000000
      0000EF3DEF3DFF7FEF3D00000000EF3DEF3D00000000E07F0000000000000000
      000000000000000000000000E07F0000000000000000EF3DEF3DEF3DEF3DEF3D
      EF3DEF3DEF3DEF3DEF3DEF3DEF3D000000000000000000000000000000000000
      000000000000F75EF75EF75EF75E000000000000000000000000000000000000
      0000EF3DEF3DFF7FFF7FFF7FFF7FEF3DEF3D0000000000000000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000EF3D00000000000000000000EF3DFF7F00000000
      00000000000000000000EF3DFF7F000000000000000000000000000000000000
      0000000000000000000000000000000000000000FF7FFF7FFF7FFF7FFF7FFF7F
      FF7FEF3DEF3DEF3DEF3DEF3DEF3DEF3DEF3D0000000000000000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000EF3D00000000000000000000EF3DFF7F00000000
      00000000000000000000EF3DFF7F000000000000000000000000000000000000
      000000000000000000000000000000000000EF3DEF3DEF3DEF3DEF3DEF3DEF3D
      EF3DEF3DEF3DEF3DEF3DEF3DEF3DEF3DEF3D0000000000000000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000EF3D00000000000000000000EF3DFF7F00000000
      00000000000000000000EF3DFF7F000000000000FF7FFF7FFF7FFF7FFF7FFF7F
      FF7F0000FF7FFF7FFF7FFF7FFF7FFF7F0000EF3DFF7F00000000000000000000
      0000EF3DFF7F00000000000000000000EF3D0000000000000000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000EF3D000000000000FF7FFF7FEF3DFF7F00000000
      00000000000000000000EF3DFF7FFF7FFF7F0000FF7FFF7FFF7FFF7FFF7FFF7F
      FF7F0000FF7FFF7FFF7FFF7FFF7FFF7F0000EF3DFF7F0000FF7FFF7F0000FF7F
      FF7FEF3DFF7FFF7FFF7FFF7FFF7FFF7FEF3DE07FE07FE07F0000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000E07FE07F0000EF3DEF3DEF3DEF3DFF7F00000000
      00000000000000000000EF3DEF3DEF3DFF7F0000FF7F00000000FF7F00000000
      F75E00001F001F001F00007C1F001F000000EF3DFF7FEF3DEF3D0000EF3DEF3D
      0000EF3DEF3DEF3DEF3DEF3DEF3DEF3DEF3D0000E07FE07F0000FF7FFF7FFF7F
      FF7FFF7FFF7FFF7FFF7F0000E07FE07FE07F0000EF3DEF3DEF3DFF7F00000000
      00000000FF7FFF7FFF7FEF3DEF3DEF3DEF3D0000FF7FFF7FFF7FFF7FFF7FFF7F
      FF7FFF7F00000000007C007C007C00000000EF3DFF7F0000FF7FFF7FFF7FFF7F
      0000FF7FEF3DFF7FEF3DEF3DEF3DFF7F00000000000000000000FF7FFF7FFF7F
      FF7F00000000000000000000000000000000000000000000EF3DFF7F00000000
      0000EF3DEF3DEF3DEF3DEF3D0000000000000000FF7F0000000000000000FF7F
      0000FF7F0000007C007C007C007C007C0000EF3DFF7FEF3DEF3DEF3DEF3D0000
      EF3D0000EF3DEF3DEF3DEF3DEF3DEF3DFF7F0000000000000000FF7FFF7FFF7F
      FF7F0000FF7FFF7F00000000000000000000000000000000EF3DFF7F00000000
      0000EF3DFF7F0000EF3DFF7F0000000000000000FF7FFF7FFF7FFF7FFF7FFF7F
      FF7FFF7F007C007C007C007C007C007C007CEF3DFF7F0000FF7FFF7F0000FF7F
      FF7FFF7FEF3DEF3DEF3DEF3DEF3DEF3DEF3D0000000000000000FF7FFF7FFF7F
      FF7F0000FF7F0000E07F0000000000000000000000000000EF3DFF7F00000000
      0000EF3DFF7FEF3DEF3DFF7FFF7F000000000000FF7F00000000FF7F00000000
      0000000000000000007C007C007C00000000EF3DFF7FEF3DEF3D0000EF3DEF3D
      EF3DEF3DEF3D0000EF3DEF3DEF3DFF7F00000000000000000000FF7FFF7FFF7F
      FF7F000000000000E07FE07F000000000000000000000000EF3DFF7FFF7FFF7F
      FF7FEF3DEF3D0000EF3DEF3DFF7FFF7F00000000FF7FFF7FFF7FFF7F0000FF7F
      FF7F000000000000007C007C007C00000000EF3DFF7F0000FF7FFF7FEF3DFF7F
      0000EF3D00000000EF3DEF3DEF3DFF7F000000000000E07F0000000000000000
      00000000000000000000E07FE07F0000000000000000EF3DEF3DEF3DEF3DEF3D
      EF3DEF3DFF7F00000000EF3DEF3DFF7FFF7F0000FF7F0000F75EFF7F0000FF7F
      000000000000EF3D007C007C007C00000000EF3DFF7FEF3DEF3D0000EF3DFF7F
      EF3D0000FF7FEF3DEF3DEF3DEF3D000000000000E07FE07F0000000000000000
      E07FE07F0000000000000000E07FE07F00000000EF3DEF3D0000000000000000
      EF3DEF3D0000000000000000EF3DEF3D00000000FF7FFF7FFF7FFF7F00000000
      0000007C007C007C007C007C000000000000EF3DFF7FFF7FFF7FFF7FEF3DEF3D
      0000EF3DEF3DEF3DEF3DEF3D000000000000E07F000000000000000000000000
      E07F0000000000000000000000000000E07FEF3D000000000000000000000000
      EF3D0000000000000000000000000000EF3D0000000000000000000000000000
      000000000000000000000000000000000000EF3DEF3DEF3DEF3DEF3DEF3D0000
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
