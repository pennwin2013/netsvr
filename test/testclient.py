#!/usr/bin/python
'''
test client
'''
import sys, os, traceback, threading, Ice, fileinput

Ice.loadSlice('callserver.ice')
import transfer

class Callback:
    def response(self, resp):
        print resp 

    def exception(self, ex):
        if isinstance(ex, Demo.RequestCanceledException):
            print "Demo.RequestCanceledException"
        else:
            print "sayHello AMI call failed:"
            print ex

            
def menu():
    print """
usage:
i: send immediate greeting
d: send delayed greeting
s: shutdown server
x: exit
?: help
"""
        
class Client(Ice.Application):
    def __init__(self,file):
        self.content = ''
        for line in fileinput.input(file):
            self.content += line
        
        fileinput.close()
        
    def run(self, args):
        if len(args) > 2:
            print self.appName() + ": too many arguments"
            return 1

        callsvr = transfer.CallServerPrx.uncheckedCast(self.communicator().propertyToProxy('endpoints'))
        if not callsvr:
            print args[0] + ": invalid proxy"
            return 1
        callsvr = callsvr.ice_timeout(10000)
        menu()

        c = None
        while c != 'x':
            req = transfer.RequestProtocol()
            req.serviceCode = 1000
            req.internalID = 0
            req.data = self.content
            
            try:
                c = raw_input("==> ")
                if c == 'i':
                    callsvr.SendMsg(req,resp)
                elif c == 'd':
                    cb = Callback()
                    r = callsvr.begin_SendMsg(req)
                    resp = callsvr.end_SendMsg(r)
                    if len(resp.data) < 1024:
                        print resp.data
                    else:
                        print "recv data length : %d" % len(resp.data)
                elif c == 's':
                    callsvr.shutdown()
                elif c == 'x':
                    pass # Nothing to do
                elif c == '?':
                    menu()
                else:
                    print "unknown command `" + c + "'"
                    menu()
            except EOFError:
                break
            except KeyboardInterrupt:
                break
            except Ice.Exception, ex:
                print ex

        return 0

app = Client(sys.argv[1])
sys.exit(app.main(sys.argv, "config.client"))

        
    