object MailCfgDlg: TMailCfgDlg
  Left = 619
  Top = 465
  BorderStyle = bsDialog
  Caption = 'Mail Server Configuration'
  ClientHeight = 303
  ClientWidth = 352
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    352
    303)
  PixelsPerInch = 120
  TextHeight = 16
  object OKBtn: TButton
    Left = 25
    Top = 258
    Width = 92
    Height = 31
    Anchors = [akLeft, akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 128
    Top = 258
    Width = 94
    Height = 31
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object ApplyButton: TButton
    Left = 233
    Top = 258
    Width = 92
    Height = 31
    Anchors = [akLeft, akBottom]
    Caption = 'Apply'
    TabOrder = 2
    OnClick = OKBtnClick
  end
  object PageControl: TPageControl
    Left = 4
    Top = 4
    Width = 342
    Height = 245
    ActivePage = DNSBLTabSheet
    TabIndex = 5
    TabOrder = 3
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object InterfaceLabel: TLabel
        Left = 9
        Top = 44
        Width = 104
        Height = 24
        AutoSize = False
        Caption = 'Interface (IP)'
        ParentShowHint = False
        ShowHint = True
      end
      object MaxClientsLabel: TLabel
        Left = 9
        Top = 76
        Width = 104
        Height = 24
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 9
        Top = 108
        Width = 104
        Height = 24
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object LinesPerYieldLabel: TLabel
        Left = 9
        Top = 140
        Width = 104
        Height = 24
        AutoSize = False
        Caption = 'Lines Per Yield'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 145
        Height = 24
        Hint = 'Automatically startup mail servers'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object NetworkInterfaceEdit: TEdit
        Left = 113
        Top = 44
        Width = 185
        Height = 24
        Hint = 
          'Enter your Network adapter'#39's static IP address here or blank for' +
          ' <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object MaxClientsEdit: TEdit
        Left = 113
        Top = 76
        Width = 48
        Height = 24
        Hint = 'Maximum number of simultaneous clients (default=10)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object MaxInactivityEdit: TEdit
        Left = 113
        Top = 108
        Width = 48
        Height = 24
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=120)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object HostnameCheckBox: TCheckBox
        Left = 185
        Top = 12
        Width = 146
        Height = 24
        Hint = 'Automatically lookup client'#39's hostnames via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DebugTXCheckBox: TCheckBox
        Left = 185
        Top = 76
        Width = 146
        Height = 24
        Hint = 'Log all transmitted mail commands and responses (for debugging)'
        Caption = 'Log TX'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object LogFileCheckBox: TCheckBox
        Left = 185
        Top = 108
        Width = 146
        Height = 24
        Hint = 'Save log entries to a file (in your DATA directory)'
        Caption = 'Log to Disk'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object LinesPerYieldEdit: TEdit
        Left = 113
        Top = 140
        Width = 48
        Height = 24
        Hint = 
          'Number of lines of message text sent/received between time-slice' +
          ' yields'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
    end
    object SMTPTabSheet: TTabSheet
      Caption = 'SMTP'
      ImageIndex = 1
      object TelnetPortLabel: TLabel
        Left = 9
        Top = 12
        Width = 104
        Height = 24
        AutoSize = False
        Caption = 'Listening Port'
      end
      object DefaultUserLabel: TLabel
        Left = 9
        Top = 76
        Width = 104
        Height = 24
        AutoSize = False
        Caption = 'Default User'
      end
      object MaxRecipientsLabel: TLabel
        Left = 9
        Top = 44
        Width = 104
        Height = 24
        AutoSize = False
        Caption = 'Max Recipients'
      end
      object SMTPPortEdit: TEdit
        Left = 113
        Top = 12
        Width = 48
        Height = 24
        Hint = 'TCP port number for incoming SMTP connections (default=25)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object UserNumberCheckBox: TCheckBox
        Left = 9
        Top = 108
        Width = 288
        Height = 24
        Hint = 
          'Allow mail to be received for users by number (e.g. "1@yourbbs.c' +
          'om")'
        Caption = 'Allow Mail Addressed by User Number'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object DebugHeadersCheckBox: TCheckBox
        Left = 185
        Top = 12
        Width = 146
        Height = 24
        Hint = 'Log all received mail headers (for debugging)'
        Caption = 'Log Headers'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DefaultUserEdit: TEdit
        Left = 113
        Top = 76
        Width = 185
        Height = 24
        Hint = 
          'Mail for unknown users will go into this user'#39's mailbox (e.g. "s' +
          'ysop")'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object AllowRelayCheckBox: TCheckBox
        Left = 9
        Top = 140
        Width = 288
        Height = 24
        Hint = 'Allow mail to be relayed for authenticated users'
        Caption = 'Allow Authenticated Users to Relay Mail'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object MaxRecipientsEdit: TEdit
        Left = 113
        Top = 44
        Width = 48
        Height = 24
        Hint = 'Maximum number of recipients for a single message'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
    end
    object POP3TabSheet: TTabSheet
      Caption = 'POP3'
      ImageIndex = 2
      object POP3PortLabel: TLabel
        Left = 9
        Top = 12
        Width = 102
        Height = 24
        AutoSize = False
        Caption = 'Listening Port'
      end
      object POP3PortEdit: TEdit
        Left = 113
        Top = 12
        Width = 48
        Height = 24
        Hint = 'TCP port number for incoming POP3 connections (default=110)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object POP3LogCheckBox: TCheckBox
        Left = 185
        Top = 12
        Width = 70
        Height = 24
        Hint = 'Log all POP3 user activity'
        Caption = 'Log'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object POP3EnabledCheckBox: TCheckBox
        Left = 256
        Top = 12
        Width = 81
        Height = 24
        Hint = 'Enable the POP3 server (requires restart of mail server)'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        OnClick = POP3EnabledCheckBoxClick
      end
    end
    object SendMailTabSheet: TTabSheet
      Caption = 'SendMail'
      ImageIndex = 3
      object RelayPortLabel: TLabel
        Left = 197
        Top = 172
        Width = 39
        Height = 26
        Alignment = taRightJustify
        AutoSize = False
        Caption = 'Port'
      end
      object DeliveryAttemptsLabel: TLabel
        Left = 9
        Top = 44
        Width = 104
        Height = 24
        AutoSize = False
        Caption = 'Max Attempts'
      end
      object RescanFreqLabel: TLabel
        Left = 167
        Top = 44
        Width = 71
        Height = 24
        Hint = 'Frequency (in seconds) of delivery attempts'
        Alignment = taRightJustify
        AutoSize = False
        Caption = 'Frequency'
      end
      object DNSRadioButton: TRadioButton
        Left = 9
        Top = 76
        Width = 104
        Height = 26
        Hint = 
          'Send mail directly to addressed mail server (requires DNS server' +
          ' access)'
        Caption = 'DNS Server'
        Checked = True
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
        TabStop = True
        OnClick = DNSRadioButtonClick
      end
      object DNSServerEdit: TEdit
        Left = 113
        Top = 76
        Width = 185
        Height = 24
        Hint = 'Host name or IP address of your ISP'#39's DNS server'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object TcpDnsCheckBox: TCheckBox
        Left = 250
        Top = 108
        Width = 65
        Height = 24
        Hint = 'Use TCP packets (instead of UDP) for DNS queries'
        Caption = 'TCP'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object RelayRadioButton: TRadioButton
        Left = 9
        Top = 140
        Width = 104
        Height = 26
        Hint = 'Route all mail through an SMTP relay server'
        Caption = 'Relay Server'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
        OnClick = DNSRadioButtonClick
      end
      object RelayServerEdit: TEdit
        Left = 113
        Top = 140
        Width = 185
        Height = 24
        Hint = 
          'Host name or IP address of external SMTP server (for relaying ma' +
          'il)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object RelayPortEdit: TEdit
        Left = 250
        Top = 172
        Width = 48
        Height = 24
        Hint = 'TCP port number for the SMTP relay server (default=25)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object DeliveryAttemptsEdit: TEdit
        Left = 113
        Top = 44
        Width = 48
        Height = 24
        Hint = 'Maximum number of delivery attempts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object RescanFreqEdit: TEdit
        Left = 250
        Top = 44
        Width = 48
        Height = 24
        Hint = 'Seconds between message base rescans'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object SendMailCheckBox: TCheckBox
        Left = 9
        Top = 12
        Width = 81
        Height = 24
        Hint = 'Enable the SendMail thread (requires restart of mail server)'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = SendMailCheckBoxClick
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 4
      object SMTPSoundLabel: TLabel
        Left = 9
        Top = 12
        Width = 101
        Height = 24
        AutoSize = False
        Caption = 'Receive Mail'
      end
      object POP3SoundLabel: TLabel
        Left = 9
        Top = 76
        Width = 101
        Height = 26
        AutoSize = False
        Caption = 'POP3 Login'
      end
      object OutboundSoundLabel: TLabel
        Left = 9
        Top = 44
        Width = 101
        Height = 24
        AutoSize = False
        Caption = 'Sending Mail'
      end
      object InboundSoundEdit: TEdit
        Left = 113
        Top = 12
        Width = 185
        Height = 24
        Hint = 'Sound file to play when inbound SMTP connections are accepted'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object InboundSoundButton: TButton
        Left = 304
        Top = 12
        Width = 25
        Height = 26
        Caption = '...'
        TabOrder = 1
        OnClick = InboundSoundButtonClick
      end
      object POP3SoundEdit: TEdit
        Left = 113
        Top = 76
        Width = 185
        Height = 24
        Hint = 'Sound file to play when accepting POP3 connections'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object POP3SoundButton: TButton
        Left = 304
        Top = 76
        Width = 25
        Height = 26
        Caption = '...'
        TabOrder = 5
        OnClick = POP3SoundButtonClick
      end
      object OutboundSoundEdit: TEdit
        Left = 113
        Top = 44
        Width = 185
        Height = 24
        Hint = 'Sound file to play when sending mail'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object OutboundSoundButton: TButton
        Left = 304
        Top = 44
        Width = 25
        Height = 26
        Caption = '...'
        TabOrder = 3
        OnClick = OutboundSoundButtonClick
      end
    end
    object DNSBLTabSheet: TTabSheet
      Caption = 'DNSBL'
      ImageIndex = 5
      ParentShowHint = False
      ShowHint = True
      object Label1: TLabel
        Left = 128
        Top = 104
        Width = 41
        Height = 16
        Caption = 'Label1'
      end
      object DNSBLServersButton: TButton
        Left = 8
        Top = 8
        Width = 113
        Height = 26
        Hint = 'DNS-based Blacklists'
        Caption = 'Blacklists'
        TabOrder = 0
        OnClick = DNSBLServersButtonClick
      end
      object DNSBLGroupBox: TGroupBox
        Left = 8
        Top = 40
        Width = 321
        Height = 161
        Caption = 'Mail from Blacklisted Servers:'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        object BLSubjectLabel: TLabel
          Left = 16
          Top = 92
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Subject'
        end
        object BLHeaderLabel: TLabel
          Left = 16
          Top = 121
          Width = 80
          Height = 26
          AutoSize = False
          Caption = 'Header'
        end
        object BLRefuseRadioButton: TRadioButton
          Left = 16
          Top = 28
          Width = 153
          Height = 24
          Hint = 'Refuse mail session with blacklisted servers'
          Caption = 'Refuse Session'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 0
          OnClick = DNSBLRadioButtonClick
        end
        object BLIgnoreRadioButton: TRadioButton
          Left = 16
          Top = 57
          Width = 161
          Height = 24
          Hint = 'Pretend to receive blacklisted mail'
          Caption = 'Silently Ignore'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 2
          OnClick = DNSBLRadioButtonClick
        end
        object BLBadUserRadioButton: TRadioButton
          Left = 168
          Top = 28
          Width = 129
          Height = 24
          Hint = 'Refuse mail address from blacklisted servers'
          Caption = 'Refuse Mail'
          TabOrder = 1
          OnClick = DNSBLRadioButtonClick
        end
        object BLTagRadioButton: TRadioButton
          Left = 168
          Top = 57
          Width = 97
          Height = 24
          Hint = 'Tag blacklisted mail with header and/or subject'
          Caption = 'Tag with:'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 3
          OnClick = DNSBLRadioButtonClick
        end
        object BLSubjectEdit: TEdit
          Left = 105
          Top = 92
          Width = 185
          Height = 24
          Hint = 'Flag to add to subject of DNS-blacklisted mail'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 4
        end
        object BLHeaderEdit: TEdit
          Left = 105
          Top = 121
          Width = 185
          Height = 24
          Hint = 'Flag to add to subject of DNS-blacklisted mail'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 5
        end
      end
      object DNSBLExemptionsButton: TButton
        Left = 128
        Top = 8
        Width = 113
        Height = 26
        Hint = 'Blacklist Exempted IP addresses'
        Caption = 'Exempt IPs'
        TabOrder = 2
        OnClick = DNSBLExemptionsButtonClick
      end
    end
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Left = 8
    Top = 480
  end
end
