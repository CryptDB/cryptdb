/*
 * prototype
 */
#include <algorithm>
#include <cryptdblearn.hh>
#include <iostream>
#include <sstream>
#include <fstream>
#include <errstream.hh>
#include <getopt.h>
#include <assert.h>
#include <onions.hh> //layout
#include <Analysis.hh> //for intersect()
#include <rewrite_main.hh>
#include <parser/sql_utils.hh>

static void help(const char *prog)
{
    std::cout << "Usage: " << prog << 
        " -u user -p password -d database [-f input file]" << "\n";
}

static bool 
ignore_line(const std::string& line)
{
    static const std::string begin_match("--");

    return(line.compare(0,2,begin_match) == 0); 
}

void 
Learn::status()
{
    std::cout << "Total queries: " << this->m_totalnum << "\n";
    std::cout << "Number of successfully executed queries: " << this->m_success_num << "\n";
}

void
Learn::trainFromFile(ProxyState &ps)
{
    std::string line;
    std::string s("");
    std::ifstream input(this->m_filename);
    assert(input.is_open() == true); 

    while(std::getline(input, line )){
        if(ignore_line(line))
            continue;

        if (!line.empty()){
            char lastChar = *line.rbegin();
            if(lastChar == ';'){
                throw CryptDBError("fix Learn::trainFromFile!");
                /*
                // FIXME: Use executeQuery.
                this->m_totalnum++;
                s += line;
                Rewriter r;
                SchemaInfo *out_schema;
                QueryRewrite qr = r.rewrite(ps, s, &out_schema);
                // FIXME.
                ResType *res =
                    qr.output->doQuery(ps.conn, ps.e_conn);
                if(res){
                    if(res->ok)
                        this->m_success_num++;
                    delete res;
                }
                if (true == qr.output->queryAgain()){ 
                    this->m_totalnum++;
                    assert(executeQuery(ps, s));
                    this->m_success_num++;
                } 
                */
                s.clear();
                continue;
            }
            s += line;
        }
    }
}
        
void 
Learn::trainFromScratch(ProxyState &ps)
{
    //TODO: implement this
    /*
     *
     * OK, here we have no queries tracing file at all and we should 
     * train using as most secure onions layout as possible.
     */
}

int main(int argc, char **argv)
{
    int c, optind = 0;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"username", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'p'},
        {"dbname", required_argument, 0, 'd'},
        {"file", required_argument, 0, 'f'},
        {NULL, 0, 0, 0},
    };

    std::string username("");
    std::string password("");
    std::string dbname("");
    std::string filename("");

    while(1)
    {
        c = getopt_long(argc, argv, "hf:u:p:d:", long_options, &optind);
        if(c == -1)
            break;

        switch(c)
        {
            case 'h':
                help(argv[0]);
                exit(0);
            case 'f':
                filename = optarg;
                break;
            case 'p':
                password = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            case 'd':
                dbname = optarg;
                break;
            case '?':
                break;
            default:
                break;
        }
    }

    assert(username != "");
    assert(password != "");
    assert(dbname != "");

    
    ConnectionInfo ci("localhost", username, password);
    const std::string master_key = "2392834";
    ProxyState ps(ci, "/var/lib/shadow-mysql", master_key);

    Learn *learn; 
    
    if(filename != "")
    {
        learn = new Learn(MODE_FILE, ps, dbname, filename);
        learn->trainFromFile(ps);
        learn->status();
    }else{
        learn = new Learn(MODE_FROM_SCRATCH, ps, dbname, "");
        learn->trainFromScratch(ps);
        learn->status();
    }
   
    delete learn;

    return 0;
}
