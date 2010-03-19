function RLoginConnection(address, port, recvfunc)
{
	this.recvfunc=recvfunc;
	this.unicode={remainder:''};

	this.onStartRequest=function(request, context) {};
	this.onStopRequest=function(request, context, status) {
		alert("Disconnected!");
		endTerm();
	};
	this.onDataAvailable=function(request, context, inputStream, offset, count) {
		this.unicode=bytesToUnicode(this.unicode.remainder+this.sock.read(count, count, 0));
		this.recvfunc(this.unicode.unicode);
	};

	this.write=function(data)
	{
		this.sock.write(data);
	}
	this.close=function()
	{
		this.sock.close();
	}
	this.connected=function()
	{
		return(this.sock.transport.isAlive());
	}

	this.sock=new Socket();
	this.sock.connect(address,port);
	this.write("\x00\x00\x00ZuulTerm/115200\x00");
	if(this.sock.read(1,1,10000)!='\x00')
		alert("No RLogin ack!");
	this.sock.asyncRead(this);
}
