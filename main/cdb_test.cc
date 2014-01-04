#include <cstdlib>
#include <cstdio>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <main/Connect.hh>

#include <readline/readline.h>
#include <readline/history.h>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <parser/embedmysql.hh>
#include <parser/stringify.hh>
#include <crypto/ecjoin.hh>
#include <util/errstream.hh>
#include <util/cryptdb_log.hh>


static inline std::string user_homedir() {
    return getenv("HOME");
}

static inline std::string user_histfile() {
    return user_homedir() + "/.cryptdb-history";
}

static void __write_history() {
    write_history(user_histfile().c_str());
}

static inline std::string &ltrim(std::string &s) {
  s.erase(s.begin(), find_if(s.begin(), s.end(), not1(std::ptr_fun<int, int>(isspace))));
  return s;
}

static inline std::string &rtrim(std::string &s) {
  s.erase(find_if(s.rbegin(), s.rend(), not1(std::ptr_fun<int, int>(isspace))).base(), s.end());
  return s;
}

static inline std::string &trim(std::string &s) {
  return ltrim(rtrim(s));
}

/** returns true if should stop, to keep looping */
static bool handle_line(ProxyState& ps, const std::string& q, bool pp=true)
{
  if (q == "\\q") {
    std::cerr << "Goodbye!\n";
    return false;
  }

  add_history(q.c_str());

  // handle meta inputs
  if (q.find(":load") == 0) {
    std::string filename = q.substr(6);
    trim(filename);
    std::cerr << RED_BEGIN << "Loading commands from file: " << filename << COLOR_END << std::endl;
    std::ifstream f(filename.c_str());
    if (!f.is_open()) {
      std::cerr << "ERROR: cannot open file: " << filename << std::endl;
    }
    while (f.good()) {
      std::string line;
      getline(f, line);
      if (line.empty())
        continue;
      std::cerr << GREEN_BEGIN << line << COLOR_END << std::endl;
      if (!handle_line(ps, line)) {
        f.close();
        return false;
      }
    }
    f.close();
    return true;
  }

  static SchemaCache schema_cache;
  try {
      const std::string &default_db =
          getDefaultDatabaseForConnection(ps.getConn());
      const EpilogueResult &epi_result =
          executeQuery(ps, q, default_db, &schema_cache, pp);
      if (QueryAction::ROLLBACK == epi_result.action) {
          std::cout << GREEN_BEGIN << "ROLLBACK issued!" << COLOR_END
                    << std::endl;
      }
      return epi_result.res_type.success();
  } catch (const SynchronizationException &e) {
      std::cout << e << std::endl;
      return true;
  } catch (const AbstractException &e) {
      std::cout << e << std::endl;
      return true;
  }  catch (const CryptDBError &e) {
      std::cout << "Low level error: " << e.msg << std::endl;
      return true;
  } catch (const std::runtime_error &e) {
      std::cout << "Unexpected Error: " << e.what() << std::endl;
      return false;
  }
}

int
main(int ac, char **av)
{
    if (ac != 3) {
        std::cerr << std::endl << "Usage: " << av[0]
                  << " <embedded-db-path> <to-be-encrypted-db-name>"
                  << std::endl << std::endl;
        exit(1);
    }

    using_history();
    read_history(user_histfile().c_str());
    atexit(__write_history);

    ConnectionInfo ci("localhost", "root", "letmein");
    const std::string master_key = "2392834";
    ProxyState ps(ci, av[1], master_key);
    const std::string create_db =
        "CREATE DATABASE IF NOT EXISTS " + std::string(av[2]);
    if (!handle_line(ps, create_db, false)) {
        return 1;
    }
    const std::string use_db = "USE " + std::string(av[2]);
    if (!handle_line(ps, use_db, false)) {
        return 1;
    }

    const std::string prompt = BOLD_BEGIN + "CryptDB=#" + COLOR_END + " ";

    for (;;) {
        char *input = readline(prompt.c_str());
        if (!input) break;
        std::string q(input);
        if (q.empty()) continue;
        else{
            if (!handle_line(ps, q))
                break;
        }
    }
}
