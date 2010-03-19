function Socket()
{
    const sockServiceClass =
        Components.classes["@mozilla.org/network/socket-transport-service;1"];

    this.sockService = sockServiceClass.getService().QueryInterface(Components.interfaces.nsISocketTransportService);
}

Socket.prototype.connect=function(addr,port,handler)
{
    this.addr = addr.toLowerCase();
    this.port = port;

    var usingHTTPCONNECT = false;

	this.transport = this.sockService.createTransport(null, 0, addr, port, null);
	if(!this.transport)
		throw("Could not create transport!");

	this.outputStream = this.transport.openOutputStream(Components.interfaces.nsITransport.OPEN_BLOCKING, 0, 0);
	if(!this.outputStream)
		throw("Could not create output stream!");
	this.sOutputStream = Components.classes["@mozilla.org/binaryoutputstream;1"].createInstance(Components.interfaces.nsIBinaryOutputStream);
	if(!this.sOutputStream)
		throw("Could not create sOutput stream!");
	this.sOutputStream.setOutputStream(this.outputStream);
	this.inputStream = this.transport.openInputStream(0/*Components.interfaces.nsITransport.OPEN_BLOCKING*/, 0, 0);
	if(!this.inputStream)
		throw("Could not create input stream!");
	this.sInputStream = Components.classes["@mozilla.org/binaryinputstream;1"].createInstance(Components.interfaces.nsIBinaryInputStream);
	if(!this.sInputStream)
		throw("Could not create Sinput stream!");
	this.sInputStream.setInputStream(this.inputStream);

    return true;
}

Socket.prototype.write=function(data)
{
	this.sOutputStream.writeBytes(data, data.length);
}

Socket.prototype.read=function(minbytes, maxbytes, timeout /* milliseconds */)
{
	var buf='';
	var total_delay=0;
	var recv=0;

	if(typeof(maxbytes) != 'number')
		maxbytes=1024*1024*1;		// One megabyte
	if(typeof(minbytes) != 'number')
		minbytes=1;
	while(buf.length < minbytes && (typeof(timeout != 'number') || total_delay < timeout)) {
		if(total_delay) {
			nsWaitForDelay(100);
			total_delay += 100;
		}
		try {
			recv=this.sInputStream.available();
			if(recv > maxbytes)
				recv=maxbytes;
			if(recv < minbytes)
				recv = minbytes;
			buf += this.sInputStream.readBytes(recv);
		}
		catch(e) {
			// Is this "Would Block"?
		}
	}
	return buf;
}

Socket.prototype.asyncRead=function(handlers)
{
	var pump = Components.classes["@mozilla.org/network/input-stream-pump;1"].createInstance(Components.interfaces.nsIInputStreamPump);
	pump.init(this.inputStream, -1, -1, 1, 1024*1024, false);
	this.handlers=handlers;
	pump.asyncRead(handlers, this);
}

Socket.prototype.close=function()
{
	this.inputStream.close();
	this.outputStream.close();
}
