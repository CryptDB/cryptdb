import os
import sys

base = sys.argv[1]
number = int(sys.argv[2])
result = sys.argv[3]

os.system("touch nc_sofar")

for i in xrange(number):
    nextone = base + repr(i)
    os.system("cat nc_sofar " + nextone + " > nc_tmp")
    os.system("mv nc_tmp nc_sofar")


os.system("mv nc_sofar " + result)
