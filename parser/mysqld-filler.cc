#include <rpl_reporting.h>
#include <rpl_rli.h>

/*
 * We need to convert libmysqld.a into a shared library.
 * Unfortunately, it has some dangling references that
 * do not normally get exposed when performing static
 * linking (because not all parts of mysql's source code
 * are archived into libmysqld.a).
 *
 * Fill in these missing functions with dummy placeholders.
 */

void __attribute__((noreturn))
Slave_reporting_capability::report(loglevel, int, char const*, ...) const
{
    abort();
}

void __attribute__((noreturn))
Relay_log_info::slave_close_thread_tables(THD*)
{
    abort();
}

/*
 * This is actually Relay_log_info::Relay_log_info(bool),
 * but defining it as a real constructor makes the compiler
 * want to initialize member fields, which leads to calls
 * to more functions that aren't defined in libmysqld.a.
 * Cut our losses by just defining the mangled symbol.
 */
extern "C" void _ZN14Relay_log_infoC1Eb() __attribute__((noreturn));

void
_ZN14Relay_log_infoC1Eb()
{
    abort();
}
