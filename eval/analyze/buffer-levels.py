#!/usr/bin/python

import sys, collections

field_ciphers = collections.defaultdict(set)
collapse_ciphers = True

while True:
    l = sys.stdin.readline()
    if l == '':
        break

    w = l.strip().split(' ')
    if len(w) < 4 or w[0] != 'FIELD' or w[2] != 'CIPHER':
        continue

    f = w[1]
    c = w[3]
    field_ciphers[f].add(c)

for f in field_ciphers:
    for c in field_ciphers[f]:
        print 'FIELD', f, 'CIPHER', c

