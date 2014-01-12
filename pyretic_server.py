import SocketServer #socket server in python 3.


class MyTCPHandler(SocketServer.BaseRequestHandler):
    """
    The RequestHandler class for our server.

    It is instantiated once per connection to the server, and must
    override the handle() method to implement communication to the
    client.
    """

    def handle(self):
	while 1:
	    # self.request is the TCP socket connected to the client
            self.data = self.request.recv(1024)
	    if not self.data:
                break
	    self.data = self.data.strip()

	    for msg in self.data:
		if msg=='1':
		    print("Client is overloaded!")
		elif msg=='2':
		    print("Client is not overloaded!")
		elif msg=='3':
		    print("Client process is up!")
		elif msg=='4':
		    print("Client process is down!")
		else:
		    print("Msg Not Recognized!")

            print("{} wrote:".format(self.client_address[0]))
            print(self.data)

if __name__ == "__main__":
    HOST, PORT = "localhost", 9002

    # Create the server, binding to localhost on port 9002
    server = SocketServer.TCPServer((HOST, PORT), MyTCPHandler)

    # Activate the server; this will keep running until you
    # interrupt the program with Ctrl-C
    server.serve_forever()
