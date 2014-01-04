#!/usr/bin/python

# inputs: repeatexp  queryfile needsreload?
import sys
import os
import time


workers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]

if len(sys.argv) != 6:
    print 'usage: repeatexp plain_queryfile enc_queryfile nolines needsreload? norepeats'
    exit(-1)

plain_queryfile = sys.argv[1]
enc_queryfile = sys.argv[2]
nolines =int(sys.argv[3])
needsreload = int(sys.argv[4])
norepeats = sys.argv[5]

plain_tput = {}
plain_lat = {}
enc_tput = {}
enc_lat = {}

responselog = "runexp_log"
os.putenv("RUNEXP_LOG_RESPONSE", responselog)

flag_ran = False

def getEnv(name):
    val = os.getenv(name, "")
    if val == "":
        print "env variable " + name + " does not exist "
        exit(-1)
    return val

def myExec(*arg):
    print arg
    
    pid = os.fork()
    
    if pid == 0:
        os.execlp(*arg)
    elif pid < 0:
        print 'failed to execute' + arg
        exit(-1)
    
    #in parent 
    os.waitpid(pid, 0)

    f = open(responselog,'r')
    
    tput = f.readline()
    lat = f.readline()
    
    f.close()
     
    return (tput, lat)

for w in workers:

    if (not flag_ran) or needsreload:
        cmd = "mysql -u root -pletmein -e 'drop database tpccplain; create database tpccplain;'"
        print cmd
        if os.system(cmd) < 0:
            print "error when dropping tpccplain"
            exit(-1)
        
        cmd = "mysql -u root -pletmein tpccplain < ../dumps/sch2_dump_plain_w1"
        print cmd
        if os.system(cmd) < 0:
            print "error when loading plain"
            exit(-1)
    
    res =  myExec("../../edb/tests/test", "test", "-d", "tpccplain", "-v", "all", "trace", "eval", plain_queryfile, repr(nolines), repr(w), norepeats, "1", "5000" )
    plain_tput[w] = res[0]
    plain_lat[w] = res[1]
    
    if (not flag_ran) or needsreload:
        cmd = "mysql -u root -pletmein -e 'drop database tpccenc; create database tpccenc;'"
        print cmd
        if os.system(cmd) < 0:
            print "error when dropping tpccenc"
            exit(-1)
        
        cmd = "mysql -u root -pletmein tpccenc < ../dumps/up_dump_enc_w1"
        print cmd
        if os.system(cmd) < 0:
            print "error when loading enc"
            exit(-1)
     
    res = myExec("../../edb/tests/test", "test", "-d", "tpccenc", "-v", "all", "trace", "eval", enc_queryfile, repr(nolines), repr(w), norepeats, "1", "5000" )
  
    enc_tput[w] = res[0]
    enc_lat[w] = res[1]
  
    flag_ran = True
    
print "Overall data:"

for w in workers:
    print "workers " + repr(w) + " plain tput " + repr(plain_tput[w]) +" enc tput " + repr(enc_tput[w]) 
    print "          plain lat " + repr(plain_lat[w]) + "   enc lat " + repr(enc_lat[w])
    

    
    

