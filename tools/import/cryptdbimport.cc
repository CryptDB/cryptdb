#include <algorithm>
#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <rewrite_main.hh>
#include <cryptdbimport.hh>

static void __attribute__((noreturn))
do_display_help(const char *arg)
{
    std::cout << "CryptDBImport" << std::endl;
    std::cout << "Use: " << arg << " [OPTIONS]" << std::endl;
    std::cout << "OPTIONS are:" << std::endl;
    std::cout << "-u<username>: MySQL server username" << std::endl;
    std::cout << "-p<password>: MySQL server password" << std::endl;
    std::cout << "-n: Do not execute queries. Only show stdout." << std::endl;
    std::cout << "-f <file>: MySQL's .sql dump file, originated from \"mysqldump\" tool." << std::endl;
    std::cout << "To generate DB's dump file use mysqldump, e.g.:" << std::endl;
    std::cout << "$ mysqldump -u user -ppassword --all-databases >dumpfile.sql" << std::endl;
    exit(0);
}


static bool 
ignore_line(const std::string& line)
{
    static const std::string begin_match("--");

    return(line.compare(0,2,begin_match) == 0); 
}

void
Import::printOutOnly(void)
{
    std::string line;
    std::string s("");
    std::ifstream input(this->filename);

    assert(input.is_open() == true); 

    while(std::getline(input, line )){
        if(ignore_line(line))
            continue;

        if (!line.empty()){
            char lastChar = *line.rbegin();
            if(lastChar == ';'){
                std::cout << s + line << std::endl;
                s.clear();
                continue;
            }
            s += line;
        }
    }
}

void
Import::executeQueries(ProxyState& ps)
{
    std::string line;
    std::string s("");
    std::ifstream input(this->filename);

    assert(input.is_open() == true); 

    while(std::getline(input, line )){
        if(ignore_line(line))
            continue;

        if (!line.empty()){
            char lastChar = *line.rbegin();
            if(lastChar == ';'){
                s += line;
                std::cout << s << std::endl;
                // FIXME
                assert(false);
                /*
                const EpilogueResult &epi_res = executeQuery(ps, s);
                assert(epi_res.res_type.success());
                s.clear();
                continue;
                */
            }
            s += line;
        }
    }
}


int main(int argc, char **argv)
{
    int c, threads = 1, optind = 0;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"inputfile", required_argument, 0, 'f'},
        {"password", required_argument, 0, 'p'},
        {"user", required_argument, 0, 'u'},
        {"noexec", required_argument, 0, 'n'},
        {"threads", required_argument, 0, 't'},
        {NULL, 0, 0, 0},
    };

    std::string username("");
    std::string password("");
    bool exec = true;

    while(1)
    {
        c = getopt_long(argc, argv, "hf:p:u:t:n", long_options, &optind);
        if(c == -1)
            break;

        switch(c)
        {
            case 'h':
                do_display_help(argv[0]);
            case 'f':
                {
                    Import import(optarg);
                    if(exec == true){
                        ConnectionInfo ci("localhost", username, password);
                        const std::string master_key = "2392834";
                        ProxyState ps(ci, "/var/lib/shadow-mysql",
                                      master_key);

                        // Execute queries
                        import.executeQueries(ps);
                    } else {
                        import.printOutOnly();
                    }
                }
                break;
            case 'p':
                password = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            case 't':
                threads = atoi(optarg);
                (void)threads;
                break;
            case 'n':
                exec = false;
                break;
            case '?':
                break;
            default:
                break;

        }
    }
    return 0;
}
