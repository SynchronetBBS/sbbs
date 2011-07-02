function ClientConnect(AHost, APort) {
  if (HtmlTerm.Loaded) {
    HtmlTerm.Connect(AHost, APort);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.Connect(AHost, APort); }
  }
}

function ClientConnected() {
  if (HtmlTerm.Loaded) {
    return HtmlTerm.Connected();
  } else {
    var FO = GetfTelnetObject();
    if (FO) { return FO.Connected(); }
  }
  
  return false;
}

function ClientDisconnect() {
  if (HtmlTerm.Loaded) {
    HtmlTerm.Disconnect();
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.Disconnect(); }
  }
}

function ClientSetBorderStyle(AStyle) {
  ClientVars.BorderStyle = AStyle;

  if (HtmlTerm.Loaded) {
  	// TODO HtmlTerm doesn't support border style yet
  	// TODO HtmlTerm.SetBorderStyle(ClientVars.BorderStyle);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.SetBorderStyle(ClientVars.BorderStyle); }
  }
}

function ClientSetFont(ACodePage, AWidth, AHeight) {
  ClientVars.CodePage = ACodePage;
  ClientVars.FontWidth = AWidth;
  ClientVars.FontHeight = AHeight;

  if (HtmlTerm.Loaded) {
    Crt.SetFont(ClientVars.CodePage, ClientVars.FontWidth, ClientVars.FontHeight);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.SetFont(ClientVars.CodePage, ClientVars.FontWidth, ClientVars.FontHeight); }
  }
}

function ClientSetScreenSize(AColumns, ARows) {
  ClientVars.ScreenColumns = AColumns;
  ClientVars.ScreenRows = ARows;

  if (HtmlTerm.Loaded) {
    Crt.SetScreenSize(ClientVars.ScreenColumns, ClientVars.ScreenRows);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.SetScreenSize(ClientVars.ScreenColumns, ClientVars.ScreenRows); }
  }
}

function fTelnetResize(AWidth, AHeight) {
  var FO = GetfTelnetObject();
  if (FO) { 
    FO.setAttribute("width", AWidth);
    FO.setAttribute("height", AHeight);
  }
}

function GetfTelnetObject() {
  var AID = "fTelnet";

  if (window.document[AID]) { return window.document[AID]; }

  if (navigator.appName.indexOf("Microsoft Internet") == -1) {
    if (document.embeds && document.embeds[AID]) { return document.embeds[AID]; } 
  } else {
    return document.getElementById(AID);
  }
}
