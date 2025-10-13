#!/usr/bin/env -S jsexec -C -n

const attrs = [
	"Private",
	"Crash",
	"Recd",
	"Sent",
	"FileAttached",
	"InTransit",
	"Orphan",
	"KillSent",
	"Local",
	"HoldForPickup",
	"unused",
	"FileRequest",
	"ReturnReceiptRequest",
	"IsReturnReceipt",
	"AuditRequest",
	"FileUpdateReq"
];

function dumpFlags(f)
{
	var gotFirst = false;
	for (flag in attrs) {
		if (f & (1 << flag)) {
			if (gotFirst)
				write(", ");
			write(attrs[flag]);
			gotFirst = true;
		}
	}
	return gotFirst;
}

function dumpMsg(fname)
{
	var f = new File(fname);
	f.network_byte_order = false;
	if (!f.open("rb"))
		throw new Error("Failed to open "+fname);
	print("\r\nDumping: "+f.name);
	var oun = f.read(36).replace(/\x00.*$/,'');
	//print("From User: "+oun);

	var dun = f.read(36).replace(/\x00.*$/,'');
	//print("To User: "+dun);

	print("Subject: "+f.read(72));
	print("DateTime: "+f.read(20));

	print("Times Read: "+f.readBin(2));

	var dno = f.readBin(2);
	//print("Destination Node: "+dno);

	var ono = f.readBin(2);
	//print("Originating Node: "+ono);

	print("Cost: "+f.readBin(2));

	var onet = f.readBin(2);
	//print("Originating Net: "+onet);

	var dnet = f.readBin(2);
	//print("Destination Net: "+dnet);

	var dz = f.readBin(2);
	//print("Destination Zone: "+dz);

	var oz = f.readBin(2);
	//print("Originating Zone: "+oz);

	var dp = f.readBin(2);
	//print("Destination Point: "+dp);

	var op = f.readBin(2);
	//print("Originating Point: "+op);

	print("In reply to: "+f.readBin(2));

	var attr = f.readBin(2);
	write("Attributes: ");
	if (dumpFlags(attr)) write(' ');
	printf("(0x%04x)\n", attr);

	print("Next Reply: "+f.readBin(2));

	print("To: "+dun+"@"+dz+":"+dnet+"/"+dno+"."+dp);
	print("From: "+oun+"@"+oz+":"+onet+"/"+ono+"."+op);
	print("Text:");
	print(f.read().replace(/[\x00-\x1f]/g, function(ch) {
		return '^'+ascii(ascii(ch)+ascii('@'));
	}).replace(/\^J/g, '\n').replace(/\^M/g, '\n'));
}

for (var f in argv)
	dumpMsg(argv[f]);
print('');
