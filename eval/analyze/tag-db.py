#!/usr/bin/python

import sys

dbmap = {}

while True:
  l = sys.stdin.readline()
  if l == '': break
  (ipsrc, ipdst, tcpsrc, tcpdst, db, q) = l.rstrip('\n').split('\t')

  dbkey = ':'.join((ipsrc, ipdst, tcpsrc, tcpdst))
  if db != '':
    dbmap[dbkey] = db
  else:
    db = dbmap.get(dbkey, '')

  if q != '':
    print db, q

