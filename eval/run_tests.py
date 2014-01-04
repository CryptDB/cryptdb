import sys
import os
import time

class stats:
    def set(self, work):
        self.worker = work
        self.total_queries = 0
        self.queries_failed = 0
        self.spassed = 0
        self.begin = 0

    def begin_timer(self):
        self.begin = time.time()

    def query_good(self, num_http):
        self.total_queries += num_http
        self.spassed = time.time() - self.begin
    
    def query_bad(self, num_http):
        self.total_queries += num_http
        self.queries_failed += 1
        self.spassed = time.time() - self.begin

    def end_timer(self):
        self.spassed = time.time() - self.begin

    
cookie_file = "cookie"
output_file = "a.html"
username = "admin"
ip = "localhost"
flags = ' -q '
VERBOSE = False
WGET_VERBOSE = False
time_ls = []

HTTP_LOGIN = 1
HTTP_READ_M = 3
HTTP_READ_P = 4
HTTP_WRITE_M = 4
HTTP_WRITE_P = 5

def main(arg):
    print "got run_tests"

def run(arg, myStat):
    #run_tests.py username no_read_email no_read_post no_write_email no_post no_overall_repeats html_filename cookie_filename
    global username
    global cookie_file
    global output_file
    global flags
    if len(arg) > 1 and arg[1] == "-h":
        print "syntax is: \n\n>python run_test.py username no_read_message no_read_post no_write_message no_write_post repeats html_filename cookie_filename readsfirst\n"
        print "username:"
        print "\tthe name of the user who we should log in as\n\tthis user must have password \'letmein\'\n\tthis user must have at least one private message to him/her\n"
        print "no_read_messages:"
        print "\tthe number of times the user will read a private message\n"
        print "no_read_post:"
        print "\tthe number of times the user will read a forum, topic and post\n"
        print "no_write_message:"
        print "\tthe number of times a user will send a private message to admin\n\tsending a private message takes a minimum of 62 seconds\n"
        print "no_write_post:"
        print "\tthe number of times a user will post to the forum\n\tposting takes a minimum of 31 seconds\n"
        print "repeats:"
        print "\tthe number of times the above actions (writing and reading posts and messages) are repeated\n\tfor example, if we have \'user 0 3 0 1 2\', user will view the forum/topic/post series three times in a row, then add a post to the forum, then view the forum/topic/post three more times and add another post\n"
        print "html_filename:"
        print "\tthe path to the temporary file where the script stores the downloads of the webpage\n\tthe file need not exist before running the script, but please do not change it while the script is running\n"
        print "cookie_filename:"
        print "\tthe path to the temporary file where the script stores the cookie for the user\n\tthe file need not exist before running the script, but please do not change it while the script is running\n"
        print "\t readsfirst if this is 1 the reads are done first"
        return
    if len(arg) != 10:
        print len(arg)
        print("Wrong number of arguments: try \n>python run_test.py -h")
        return -1 
    if WGET_VERBOSE:
        flags = ' '
    username = arg[1]
    read_m = int(arg[2])
    read_p = int(arg[3])
    write_m = int(arg[4])
    write_p = int(arg[5])
    repeat = int(arg[6])
    output_file = arg[7]
    cookie_file = arg[8]
    readsfirst = int(arg[9])

    if VERBOSE: print "logging in"
    login()
    if not login_good():
        myStat.query_bad(HTTP_LOGIN)
        print "ERROR: could not login\n\tdoes this user have the password letmein?"
        return -1
    else:
        myStat.query_good(HTTP_LOGIN)
    myStat.begin_timer()
    for i in range(0,repeat):
        #time_ls.append(time2-time1)
        if readsfirst:
            for j in range(0,read_m):
                if VERBOSE: print "reading message!"
                read_message()
                if not read_message_ok():
                    myStat.query_bad(HTTP_READ_M)
                    print "ERROR: messages not read\n\tdoes "+username+" have message?"
                    return -1
                else:
                    myStat.query_good(HTTP_READ_M)
            for j in range(0,read_p):
                if VERBOSE: print "reading post!"
                read_post()
                if not read_post_ok():
                    myStat.query_bad(HTTP_READ_P)
                    print "ERROR: post not read\n\tdoes this forum have posts?\n\tdoes "+username+" have permission to access this forum?"
                    return -1
                else:
                    myStat.query_good(HTTP_READ_P)
            for j in range(0,write_m):
                if VERBOSE: print "writing message!"
                write_message()
                if not write_message_ok():
                    myStat.query_bad(HTTP_WRITE_M)
                    print "ERROR: message not written\n\tdoes "+username+" have permission to write a message?"
                    return -1
                else:
                    myStat.query_good(HTTP_WRITE_M)
            for j in range(0,write_p):
                if VERBOSE: print "writing post!"
                write_post()
                if not write_post_ok():
                    myStat.query_bad(HTTP_WRITE_P)
                    print "ERROR: post not written\n\tdoes "+username+" have permission to post in this forum?"
                    return -1
                else:
                    myStat.query_good(HTTP_WRITE_P)
        else:
             for j in range(0,write_m):
                if VERBOSE: print "writing message!"
                write_message()
                if not write_message_ok():
                    myStat.query_bad(HTTP_WRITE_M)
                    print "ERROR: message not written\n\tdoes "+username+" have permission to write a message?"
                    return -1
                else:
                    myStat.query_good(HTTP_WRITE_M)
             for j in range(0,write_p):
                if VERBOSE: print "writing post!"
                write_post()
                if not write_post_ok():
                    myStat.query_bad(HTTP_WRITE_P)
                    print "ERROR: post not written\n\tdoes "+username+" have permission to post in this forum?"
                    return -1
                else:
                    myStat.query_good(HTTP_WRITE_P)
             for j in range(0,read_m):
                if VERBOSE: print "reading message!"
                read_message()
                if not read_message_ok():
                    myStat.query_bad(HTTP_READ_M)
                    print "ERROR: messages not read\n\tdoes "+username+" have message?"
                    return -1
                else:
                    myStat.query_good(HTTP_READ_M)
             for j in range(0,read_p):
                if VERBOSE: print "reading post!"
                read_post()
                if not read_post_ok():
                    myStat.query_bad(HTTP_READ_P)
                    print "ERROR: post not read\n\tdoes this forum have posts?\n\tdoes "+username+" have permission to access this forum?"
                    return -1
                else:
                    myStat.query_good(HTTP_READ_P)
    myStat.end_timer()
    return 0



def login():
    login = "wget"+flags+"--save-cookies="+cookie_file+" --post-data=\'username="+username+"&password=letmein&redirect=.%2Fucp.php%3Dlogin&sid=nonsense&redirect=index.php&login=Login\' \'http://"+ip+"/phpBB3/ucp.php?mode=login\' -O "+output_file
    os.system(login)
    return

def read_forum():
    index = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/index.php\' -O "+output_file
    os.system(index)
    forum = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/viewforum.php?f=2\' -O "+output_file
    os.system(forum)
    topic = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/viewtopic.php?f=2&t=1\' -O "+output_file
    os.system(topic)
    return

def read_post():
    read_forum()
    time1 = time.time()
    post = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/viewtopic.php?f=2&t=1#p=1\' -O "+output_file
    os.system(post)
    #time2 = time.time()
    #if VERBOSE: print time2-time1
    #time_ls.append(time2-time1)
    return

def write_post():
    read_forum()
    sid = parse_sid(cookie_file)
    form_token = ""
    creation_time = ""
    last_click = ""
    #time1 = time.time()
    post_form = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/posting.php?mode=reply&f=2&t=1\' -O "+output_file
    os.system(post_form)
    (form_token,creation_time,last_click) = parse_tokens(output_file)
    post = "wget"+flags+"--keep-session-cookies --load-cookie="+cookie_file+" --save-cookies="+cookie_file+" --post-data=\'icon=30&subject=test&addbbcode20=100&message=This is a test post which contains multiple sentences.  They are not, however, very interesting sentences.&topic_cur_post_id=0&lastclick="+last_click+"&post=\"Submit\"&creation_time="+creation_time+"&form_token="+form_token+"\' \'http://"+ip+"/phpBB3/posting.php?mode=reply&f=2&t=1&sid="+sid+"\' -O "+output_file
    os.system(post)
    #time2 = time.time()
    #if VERBOSE: print(time2-time1)
    #time_ls.append(time2-time1-1)
    return

def read_ucp():
    ucp = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/ucp.php\' -O "+output_file
    os.system(ucp)
    return

def read_message():
    global time_ls
    read_ucp()
    inbox = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/ucp.php?i=pm&folder=inbox\' -O "+output_file
    os.system(inbox)
    mid = parse_message(output_file)
    #time1 = time.time()
    message = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/ucp.php?i=pm&mode=view&f=0&p="+mid+"\' -O "+output_file
    os.system(message)
    #time2 = time.time()
    if VERBOSE: print time2-time1
    #time_ls.append(time2-time1)
    return

def write_message():
    read_ucp()
    sid = parse_sid(cookie_file)
    #time1 = time.time()
    message_form = "wget"+flags+"--load-cookie="+cookie_file+" --save-cookies="+cookie_file+" \'http://"+ip+"/phpBB3/ucp.php?i=175\' -O "+output_file
    os.system(message_form)
    (form_token,creation_time,last_click) = parse_tokens(output_file)
    sid = parse_sid(cookie_file)
    message_write = "wget"+flags+"--keep-session-cookies --load-cookie="+cookie_file+" --save-cookies="+cookie_file+" --post-data=\'username_list=admin&icon=0&subject=hello world&addbbcode20=100&message=Hello, admin!  \nI bet you are getting a lot of this exact same message.  :-P&lastclick="+last_click+"&status_switch=0&post=Submit&creation_time="+creation_time+"&form_token="+form_token+"\' \'http://"+ip+"/phpBB3/ucp.php?i=175&sid="+sid+"\' -O "+output_file
    os.system(message_write)
    (form_token,creation_time,last_click) = parse_tokens(output_file)
    sid = parse_sid(cookie_file)
    message_send = "wget"+flags+"--keep-session-cookies --load-cookie="+cookie_file+" --save-cookies="+cookie_file+" --post-data=\'username_list=&icon=0&subject=hello world&addbbcode20=100&message=Hello, admin!  \nI bet you are getting a lot of this exact same message.  :-P&address_list%5Bu%5D%5B2%5D=to&lastclick="+last_click+"&status_switch=0&post=Submit&creation_time="+creation_time+"&form_token="+form_token+"\' \'http://"+ip+"/phpBB3/ucp.php?i=175&sid="+sid+"\' -O "+output_file
    os.system(message_send)
    #time2 = time.time()
    #if VERBOSE: print time2-time1
    #time_ls.append(time2-time1-4)
    return


def login_good():
    f = open(output_file,'r')
    line = f.readline()
    if line == "":
        return False
    while(line != ""):
        if (line.find("You have been successfully logged in.") != -1):
            return True
        line = f.readline()
    return False

def read_message_ok():
    f = open(output_file,'r')
    line = f.readline()
    if line == "":
        return False
    while(line != ""):
        if (line.find("No messages") != -1):
            return False
        line = f.readline()
    return True

def read_post_ok():
    f = open(output_file,'r')
    line = f.readline()
    if line == "":
        return False
    while(line != ""):
        if (line.find("You are not authorised to read this forum.") != -1):
            return False
        line = f.readline()
    return True

def write_message_ok():
    f = open(output_file,'r')
    line = f.readline()
    while(line != ""):
        if (line.find("This message has been sent successfully") != -1):
            return True
        line = f.readline()
    return False

def write_post_ok():
    f = open(output_file,'r')
    line = f.readline()
    while(line != ""):
        if (line.find("This message has been posted successfully") != -1):
            return True
        line = f.readline()
    return False








def parse_sid(filename):
    f = open(filename,'r')
    line = f.readline()
    while(line != "" and (line.startswith('#') or line.strip() == "")):
        line = f.readline()
    while(line != ""):
        parts = line.split();
        if parts[5].find("sid") > 0:
            return parts[6]
        line = f.readline()
    return None


def parse_tokens(filename):
    f = open(filename,'r')
    last = ""
    create = ""
    token = ""
    line = f.readline()
    while(line != ""):
        #name="lastclick" value="time"
        if(line.find("lastclick") != -1):
            temp = line.partition("lastclick")
            temp = temp[2].partition("value=")
            temp = temp[2].partition("/>")
            last = temp[0].strip().strip(' \"/>').strip()
        #name="creation_time" value="time"
        if(line.find("creation_time") != -1):
            temp = line.partition("creation_time")
            temp = temp[2].partition("value=")
            create = temp[2].strip().strip(' \"/>').strip()
        #name="form_token" value="token"
        if(line.find("form_token") != -1):
            temp = line.partition("form_token")
            temp = temp[2].partition("value=")
            token = temp[2].strip().strip(' \"/>').strip()
        line = f.readline()
    return (token,create,last)

def parse_message(filename):
    f = open(filename,'r')
    line = f.readline()
    while(line != ""):
        if (line.find("ucp.php?i=pm&amp;mode=view&amp;f=0&amp;p=") != -1):
            temp = line.partition("&amp;p=")
            temp = temp[2].partition("\"")
            return temp[0]
        line = f.readline()
    return "0"

main(sys.argv)
