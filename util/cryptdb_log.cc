#include <util/cryptdb_log.hh>

uint64_t cryptdb_logger::enable_mask = //0;
//    cryptdb_logger::mask(log_group::log_debug) |
//    cryptdb_logger::mask(log_group::log_cdb_v) |
//    cryptdb_logger::mask(log_group::log_wrapper) |
    //   cryptdb_logger::mask(log_group::log_encl) |
//    cryptdb_logger::mask(log_group::log_edb_v) |
    //cryptdb_logger::mask(log_group::log_am_v) |
    // cryptdb_logger::mask(log_group::log_test) |
    cryptdb_logger::mask(log_group::log_warn);

