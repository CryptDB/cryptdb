import os
import sys
import time

# python test port trace

arg = sys.argv

if len(arg) != 3:
    print "wrong args: test port trace"
    sys.exit(1)


port = arg[1]
tracefile = arg[2]
query = "mysql -u root -pletmein -h 127.0.0.1 -P"+ port + " < " 
os.system(query+tracefile)

print "start timer"
start = time.time()
os.system(query+" queries")
end = time.time()

print "time is " + repr(end-start)