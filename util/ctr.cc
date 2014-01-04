#include <util/ctr.hh>

static tsc_ctr tsc;
decltype(perf_cg) perf_cg = ctrgroup(&tsc);

