import MySQLdb
import xconfig as config
import time

# tables used for grading
db_schemas = [
# grade entries for each user.  Most recent entry
# for each action is the current one.
# Possible action,value pairs:
#   submit,--
#   grade,points
#   checkoff,NULL/yes
#   extension,NULL/staff/student
#   note,text comment
#  timestamps are YYYYMMDDHHMMSS
"""create table if not exists xgrades (
  username varchar(32) not null,
  assignment char(4) not null,
  action char(16) not null,
  submitter varchar(32),
  timestamp char(14),
  value varchar(256),
  index grades_index (username)
);""",
    ]

saved_cursor = None
saved_connection = None

def open_db():
    global saved_cursor,saved_connection
    if saved_cursor is None:
        host,user,passwd,db = config.database
        saved_connection = MySQLdb.connect(host=host,user=user,passwd=passwd,db=db)
        saved_cursor = saved_connection.cursor()
        for cmd in db_schemas:
            saved_cursor.execute(cmd)
    return saved_cursor

def commit():
    if saved_connection:
        saved_connection.commit()

# returns dictionary: email -> assignments_dict
# assignments_dict: assignment -> grade_entry
# grade_entry = [points, submit_time, checkoff_time,extension,action_list]
#   points is a float or None
#   submit_time is timestamp of latest submission by student (or None)
#   checkoff_time is timestamp of checkoff (or None)
#   extension is None, 'student', 'staff'
#   action_list is a list of (action,submitter,timestamp,value)
def build_grades_dict(who=None,assignment=None):
    cursor = open_db()
    grades = {}

    # run through grade entries in order of entry_time
    # so most recent action is processed last
    cmd = 'select username, assignment, action, submitter, timestamp, value from xgrades'
    if who and assignment:
        cmd += ' where username="%s" and assignment="%s"' % (who,assignment)
    elif who:
        cmd += ' where username="%s"' % who
    elif assignment:
        cmd += ' where assignment="%s"' % assignment
    cmd += ' order by timestamp'
    cursor.execute(cmd)
    for (username,assignment,action,submitter,timestamp,value) in cursor.fetchall():
        adict = grades.get(username,None)
        if adict is None:
            adict = {}
            grades[username] = adict
        grade_entry = adict.get(assignment,[None,None,None,None,[]])
        grade_entry[4].append((action,submitter,timestamp,value))
        if action == 'submit':
            grade_entry[1] = timestamp
        elif action == 'grade':
            grade_entry[0] = float(value) if value else None
        elif action == 'checkoff':
            grade_entry[2] = timestamp if value else None
        elif action == 'extension':
            grade_entry[3] = value
        elif action == 'note':
            pass
        adict[assignment] = grade_entry

    return grades

def add_action(username,assignment,action,submitter,value,timestamp=None):
    if timestamp is None:
        timestamp = time.strftime('%Y%m%d%H%M%S')
    cursor = open_db()
    cursor.execute('insert into xgrades (username, assignment, action, submitter, timestamp, value) values (%s, %s, %s, %s, %s, %s)',(username,assignment,action,submitter,timestamp,value))
    commit()
