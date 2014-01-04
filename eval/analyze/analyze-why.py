#!/usr/bin/python

import sys, collections, multiprocessing, gzip

def do_parse(fn):
    field_info = collections.defaultdict(set)
    f = gzip.open(fn)
    while True:
        l = f.readline()
        if l == '':
            return field_info

        w = l.strip().split(' ')
        if len(w) < 4 or w[0] != 'FIELD' or w[2] != 'CIPHER' or w[3] != enctype:
            continue

        field = w[1]
        cipher = w[3]
        reason = w[6]
        field_info[field].add(reason)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print 'Usage:', sys.argv[0], 'enctype-to-explain parsed-all/*.gz'
        sys.exit(0)

    enctype = sys.argv[1]
    files = sys.argv[2:]

    p = multiprocessing.Pool(processes = len(files))

    merged = collections.defaultdict(set)
    for field_info in p.map(do_parse, files):
        for field in field_info:
            merged[field].update(field_info[field])

    set_to_fieldcount = collections.defaultdict(int)
    for xset in merged.itervalues():
        set_to_fieldcount[str(sorted(xset))] += 1

    for x in set_to_fieldcount:
        print '%9d' % set_to_fieldcount[x], x

