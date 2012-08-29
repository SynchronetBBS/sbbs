function ClientClrScr() {
  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
  	Crt.ClrScr();
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.ClrScr(); }
  }
}

function ClientConnect(AHost, APort) {
  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
    HtmlTerm.Connect(AHost, APort);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.Connect(AHost, APort); }
  }
}

function ClientConnected() {
  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
    return HtmlTerm.Connected();
  } else {
    var FO = GetfTelnetObject();
    if (FO) { return FO.Connected(); }
  }
  
  return false;
}

function ClientDisconnect() {
  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
    HtmlTerm.Disconnect();
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.Disconnect(); }
  }
}

function ClientSetFont(ACodePage, AWidth, AHeight) {
  ClientVars.CodePage = ACodePage;
  ClientVars.FontWidth = AWidth;
  ClientVars.FontHeight = AHeight;

  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
    Crt.SetFont(ClientVars.CodePage, ClientVars.FontWidth, ClientVars.FontHeight);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.SetFont(ClientVars.CodePage, ClientVars.FontWidth, ClientVars.FontHeight); }
  }
}

function ClientSetLocalEcho(ALocalEcho) {
  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
    //TODO Add local echo support to HtmlTerm Crt.LocalEcho = ALocalEcho;
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.SetLocalEcho(ALocalEcho); }
  }
}

function ClientSetScreenSize(AColumns, ARows) {
  ClientVars.ScreenColumns = AColumns;
  ClientVars.ScreenRows = ARows;

  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
    Crt.SetScreenSize(ClientVars.ScreenColumns, ClientVars.ScreenRows);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.SetScreenSize(ClientVars.ScreenColumns, ClientVars.ScreenRows); }
  }
}

function ClientSetVirtualKeyboardWidth(ANewWidth) {
  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
    //TODO Add virtual keyboard to HtmlTerm
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.SetVirtualKeyboardWidth(ANewWidth); }
  }
}

function ClientWrite(AText) {
  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
  	Crt.Write(AText);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.Write(AText); }
  }
}

function ClientWriteLn(AText) {
  if ((typeof(HtmlTerm) !== "undefined") && HtmlTerm.Loaded) {
  	Crt.WriteLn(AText);
  } else {
    var FO = GetfTelnetObject();
    if (FO) { FO.WriteLn(AText); }
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
