// $Id$

if(this.client)
	socket=client.socket;
else
	socket=new Socket("test");
printf("sendbuf = %d\r\n",socket.getoption("SNDBUF"));
printf("recvbuf = %d\r\n",socket.getoption("RCVBUF"));
printf("tcp_nodelay = %d\r\n",socket.getoption("TCP_NODELAY"));
printf("keepalive = %d\r\n",socket.getoption("KEEPALIVE"));