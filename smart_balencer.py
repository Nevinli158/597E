# SmartBalancer                                                                #
#                                                                              #
# author: Raghav Sethi (raghavs@cs.princeton.edu)                              #
# author: Nevin Li  (nhli@cs.princeton.edu)                                    #
# Based on simple_ui_firewall.py by Joshua Reich and Nick Feamster             #
################################################################################

from pox.lib.addresses import EthAddr

from pyretic.lib.corelib import *
from pyretic.lib.std import *
from pyretic.lib.query import *
from pyretic.modules.mac_learner import mac_learner

import SocketServer
import threading

class smart_balancer(DynamicPolicy):

    def __init__(self):
        # Initialize the load balancer
        print "Initializing SmartBalancer.."
        # This dictionary tracks the hosts that different IPs have been assigned
        self.existing_connections = {}
        # This dictionary tracks host status
        self.hosts = {'10.0.0.1':[0,True,True], '10.0.0.2':[0,True,False], '10.0.0.3':[0,True,False]}
        super(smart_balancer,self).__init__(true)
        
        self.ui = threading.Thread(target=self.ui_loop)
        self.ui.daemon = True
        self.set_initial_state()
        self.ui.start()

    def set_initial_state(self):
        # Set a query for the first packet fo every flow
        self.query = packets(limit=1, group_by=['srcip'])
        self.query.register_callback(self.balance)

        # Set the rewrite policy such that every packet originating from the load balanced hosts
        # appears to be from 10.0.0.100 (the canonical IP)
        self.rewrite_policy = (match(srcip=IPAddr("10.0.0.1")) + match(srcip=IPAddr("10.0.0.2")) + match(srcip=IPAddr("10.0.0.3"))) >> modify(srcip=IPAddr("10.0.0.100"))

        # Set the initial forwarding policy to send everything to 10.0.0.1 
        self.fwd_policy = match(dstip="10.0.0.100") >> modify(dstip=IPAddr("10.0.0.1"))
        
        # Whether the inital policy is still in effect or not
        self.init_policy_in_effect = True
        self.update_policy()

    def balance(self, pkt):
        
        # If the packet originates from load balanced hosts, rewrite srcip to be canonical IP
        if str(pkt['srcip']) in ["10.0.0.1", "10.0.0.2", "10.0.0.3"]:
            print "OUTGOING:", pkt['srcip'], ">>>", pkt['dstip']
            return

        print "INCOMING:", pkt['srcip'], ">>>", pkt['dstip']

        # If the source has already been assigned a host, make sure that flows go to the same host
        if str(pkt['srcip']) in self.existing_connections:
            print "NOTE:     ", pkt['srcip'], ">>>", pkt['dstip'], "already exists" 
            return

        # Assign a host to flows from this srcip
        
        assigned_host = -1

        for host, status in self.hosts.items():
            if status[1] and status[2]:
                assigned_host = host
                break

        if assigned_host == -1:
            for host, status in self.hosts.items():
                if not status[2]:
                    self.hosts[host][2] = True
                    assigned_host = host

        print "ASSIGNED:", pkt['srcip'], "to", assigned_host
        self.existing_connections[str(pkt['srcip'])] = assigned_host

        # Update the forwarding policy such that all flows from this srcip go to the assigned host
        self.update_fwdpolicy(str(pkt['srcip']), assigned_host)
        self.update_policy()

    def update_fwdpolicy (self, srcip,assigned_host):

        # If the initial policy is in effect, change the policy completely
        if self.init_policy_in_effect:
            self.fwd_policy = (match(srcip=IPAddr(srcip)) >> modify(dstip=IPAddr(assigned_host)))
            self.init_policy_in_effect = False
            return

        # Else add another parallel policy
        self.fwd_policy = self.fwd_policy + (match(srcip=IPAddr(srcip)) >> modify(dstip=IPAddr(assigned_host)))
    
    def update_policy(self):
        self.policy = self.query + self.fwd_policy + self.rewrite_policy

    def ui_loop (self):
        while(True):
            nb = raw_input('(s) change server status, or (q)uit? ')
            print self.hosts, self.existing_connections
            
            if nb == 's':
                try:
                    nb = raw_input('enter MAC address, load_status, online_status ')
                    print nb
                    (mac,load,online) = nb.split(',')
                    print mac,load,online
                    self.hosts[mac][1]=eval(load)
                    self.hosts[mac][2]=eval(online)
                    print self.hosts
                except:
                    print "Invalid Format"
            elif nb == 'q':
                print "Quitting"
                import os, signal
                os.kill(os.getpid(), signal.SIGINT)
                return
            else:
                print "Invalid option"
            

class SmartBalancerTCPServer(SocketServer.TCPServer):
    def __init__(self, server_address, RequestHandlerClass, smart_balancer, bind_and_activate=True):
	self.smart_balancer = smart_balancer
	SocketServer.TCPServer.__init__(self, server_address, RequestHandlerClass, bind_and_activate=True)

class MyTCPHandler(SocketServer.BaseRequestHandler):
    def handle(self):
        while 1:
            # self.request is the TCP socket connected to the client
            self.data = self.request.recv(1024)
            if not self.data:
                break
            self.data = self.data.strip()

	    hostArray = self.server.smart_balancer.hosts['10.0.0.1']
            for msg in self.data:
                if msg=='1':
                    print("Client is overloaded!")
		    hostArray[1] = False
                elif msg=='2':
                    print("Client is not overloaded!")
		    hostArray[1] = True
                elif msg=='3':
                    print("Client process is down!")
		    hostArray[2] = False
                elif msg=='4':
                    print("Client process is up!")
		    hostArray[2] = True
                else:
                    print("Msg Not Recognized!")

            print("{} wrote:".format(self.client_address[0]))
            print(self.data)		

def start_server (args):		
    args.serve_forever()            
            
def main ():
    HOST, PORT = "localhost", 9002

    my_smart_balancer = smart_balancer()
    server = SmartBalancerTCPServer((HOST,PORT), MyTCPHandler, my_smart_balancer)
    serverThread = threading.Thread(target=start_server, args=(server,))
    serverThread.daemon = True
    serverThread.start()
    
    return my_smart_balancer >> mac_learner()
