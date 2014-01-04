#!/usr/bin/env python

import cgi,fcntl,glob,math,os,sys,time,urllib
import MySQLdb
import xgrades_db as grades_db

# render YYYYMMDDHHMMSS timestamp in a readable form
months = ('Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec')
def display_timestamp(timestamp,secs=False):
   date = "%s-%s-%s" % (months[int(timestamp[4:6])-1],timestamp[6:8],timestamp[:4])
   time = " at %s:%s" % (timestamp[8:10],timestamp[10:12])
   if secs: time += ":%s" % timestamp[12:-1]
   return date+time

# increment YYYYMMDDHHMMSS timestamp by specified number of days
def increment_timestamp(timestamp,days):
   # convert timestamp into seconds since the epoch
   t = time.mktime(time.strptime(timestamp,'%Y%m%d%H%M%S'))
   # increment by specified number of days
   t += days * 24 * 60 * 60;
   # convert back into timestamp
   return time.strftime('%Y%m%d%H%M%S',time.localtime(t))

# access grade entry in student's grade dictionary
def get_grade_info(student_grades,assignment):
   return student_grades.get(assignment,[None,None,None,None,[]])

# count student-requested extensions over all assignments
def count_extensions(student_grades):
   global assignments
   count = 0
   for a,tpts,fpct,issued,due in assignments:
       points,submit_time,checkoff_time,extension,action_list = get_grade_info(student_grades,a)
       if extension=='student': count += 1
   return count

# timestamp for the current time
def now():
   return time.strftime('%Y%m%d%H%M%S')

# return (None or float points, boolean late)
def compute_score(assignment,tpts,due,points,submit_time,checkoff_time,extension):
   if assignment[0] == 'Q':
      # quizzes are never late...
      return (points,False)
   if points:
      # use specified submit and checkoff timestamps, if available
      # otherwise use current time for late computation
      submit = submit_time if submit_time else now()
      checkoff = checkoff_time if checkoff_time else now()
      if extension:
         late = False
      else:
         checkoff_due = increment_timestamp(due,5)
         late = submit > due #or checkoff > checkoff_due
      if late: points = 0.5 * points
   else:
      late = False
   return (points,late)

# construct a url to ourselves with arguments, passed as a dict
def url_arg(args,cgi='grades'):
   alist = []
   for (name,value) in args.items():
      alist.append("%s=%s" % (name,urllib.quote_plus(value)))
   return cgi + ".cgi?" + '&'.join(alist)

def print_histogram(h,height=200):
    keys = h.keys()
    if len(keys) == 0: return 'no data'
    keys.sort()
    min_score = keys[0]
    max_score = keys[-1]
    max_count = max([h[key] for key in keys])
    height /= max_count

    result = '<table cellspacing="0" cellpadding="0" border="0">\n<tr valign="bottom" align="center">'
    for v in range(min_score,max_score+1):
        if h.has_key(v):
           result += '<td>'
           result += '<br>'.join(['<img src="blue.png" style="width:12px; height:%dpx; margin:1px 1px 0px 0px">' % height
                                  for i in xrange(h[v])])
           result += '</td>'
        else:
           result += '<td><img src="blue.png" style="width:12px; height:0px; margin:1px 1px 0px 0px"></td>'
    result += '</tr>\n<tr align="center">'
    for v in range(min_score,max_score+1):
       if v & 1 == 0:
          result += '<td><font size="1">%d</font></td>' % v
       else:
          result += '<td><font size="1">&nbsp;</font></td>'
    result += '</tr></table>\n'
    return result

# output stats for assignments
def statistics():
   global grades
   stats = False
   print '<p>Stats for the assignments:<p>'
   # generate stats for each assignment in turn
   for a,tpts,fpct,issued,due in assignments:
      n = 0
      sum = 0
      sumsq = 0
      # look through each student in grades dict
      for student in grades.keys():
         g = grades[student]
         # if student has a grade for this assignment
         # add in their contribution to the statistics
         if g.has_key(a):
            points,submit_time,checkoff_time,extension,action_list = g[a]
            score,late = compute_score(a,tpts,due,points,submit_time,checkoff_time,extension)
            if score:
               n += 1.0
               sum += score
               sumsq += score*score
      # if assignment has some grades, print out stats
      if n != 0:
         if not stats:
            stats = True
            print '<p>'
         mean = sum/n
         if n > 1:
            stddev = math.sqrt((sumsq - n*mean*mean)/(n-1))
         else:
            stddev = 0
         print '%s: n=%d, avg=%0.2f, stddev=%0.2f (' % (a,n,mean,stddev)
         #print '<A href="%s">missing</A>' % url_arg({'missing':a})
         #print '<A href="%s">timeline</A>' % url_arg({'timeline':a})
         print '<A href="%s">histogram</A>' % url_arg({'histogram':a})
         print ')<br>'

# output HTML grades table
def grades_table(role):
   global grades
   grades = grades_db.build_grades_dict()

   print """<div style="width:600px">
   <p>To complete a checkoff, enter an extension or change a grade, click on student\'s name.
   <p>To grade a submitted pset, click on entry in appropriate row and column.
   <p>Assignments with an extension are shown with a yellow background
   and those with a late penalty are shown with a reddish background. Completed checkoffs
   are indicated by &radic;.
   </div>
   """

   print '<p><table border="1" style="border-collapse:collapse;" cellpadding="2" bgcolor="white">'

   # header row
   print '  <tr>'
   print '    <th> # </th>'
   print '    <th> Student </th>'
   print '    <th> Interview </th>'
   print '    <th> Grader </th>'
   for a,tpts,fpct,issued,due in assignments:
      if role=='staff':
         if a[0] == 'Q':
            span = 1
            text = '<A href="grades.cgi?assignment=%s&bulk_entry=1">%s</A>' % (a,a)
         else:
            span = 2
            text = a
         print '    <th colspan="%d">%s</th>' % (span,text)
   print '  </tr>'

   # output table alphabetically by student name
   slist = [(email,info[0],info[3],info[4]) for email,info in users.items() if info[1]=='student']
   slist.sort(lambda x,y: cmp(x[1],y[1]))
   row = 0
   for (email,name,ta,grader) in slist:
      bgcolor = '#D0FFD0' if row % 2 == 0 else '#FFFFFF'
      print '  <tr bgcolor="%s">' % bgcolor
      # student name (with link to grade entry)
      url = url_arg({'student':email})
      print '    <td><font color="grey">%d</font></td>' % (row+1)
      print '    <td><a href="%s">' % url,name,'</a></td>'
      print '    <td>',ta.split('@')[0],'</td>'
      print '    <td>',grader.split('@')[0],'</td>'
      student_grades = grades.get(email,{})
      # add column for each assignment
      if role=='staff':
         for a,tpts,fpct,issued,due in assignments:
            points,submit_time,checkoff_time,extension,action_list = get_grade_info(student_grades,a)
            score,late = compute_score(a,tpts,due,points,submit_time,checkoff_time,extension)
            if a[0] == 'Q':
               score = '%4.2f' % score if score else '&nbsp;'
               print '    <td align="right">%s</td>' % score
            else:
               if late: xbgcolor = '#FFD0D0'
               elif extension: xbgcolor = '#FFFFD0'
               else: xbgcolor = bgcolor
               score = '%4.2f' % score if score else 'grade' if submit_time else '&nbsp;'
               if submit_time:
                  print '    <td bgcolor="%s" align="right"><A href="pset.cgi?_assignment=%s&_student=%s">%s</A></td>' % (xbgcolor,a,email,score)
               else:
                  print '    <td bgcolor="%s" align="right">%s</td>' % (xbgcolor,score)
               print '    <td>%s</td>' % ('&radic;' if checkoff_time else '&nbsp;')
      print '  </tr>'
      row = row + 1

   print '</table>'   

# display list of files in the "course_only" directory
def course_only_files():
   files = glob.glob(os.path.join(course_dir,'course_only','*'))
   files.sort()
   if len(files) > 0:
      print '<p>Files available only to course participants:<ul>'
      for f in files:
         name = os.path.basename(f)
         link = '<A href="%s" target="_blank">%s</A>' %  (url_arg({'filename': name, 'action': 'ViewCourse'}),name)
         print '<li>',link
      print '</ul>'

# make a list of links to files meeting the specified criteria
def file_list(student,assignment,staff=False):
   fname = '%s-%s-' % (student,assignment)
   files = glob.glob(os.path.join(course_dir,'files',fname + '*'))
   result = []
   for file in files:
      name = os.path.basename(file)
      short_name = name[len(fname):]
      link = '<A href="%s" target="_blank">%s</A>' %  (url_arg({'filename': name, 'action': 'View'}),short_name)
      result.append(link)
   return "<br>".join(result)

# send submitted text file to browser
def send_text_file(fname,dir='files'):
   message = None
   try:
      # guess file type from extension
      base = os.path.basename(fname)
      ext_index = base.rfind('.')
      if ext_index == -1: ext = ''
      else: ext = base[ext_index+1:]
      if ext in ('jpeg','jpg'): ftype = 'image/jpeg'
      elif ext == 'png': ftype = 'image/png'
      elif ext == 'gif': ftype = 'image/gif'
      elif ext == 'svg': ftype = 'image/svg+xml'
      elif ext == 'py': ftype = 'text/plain'
      elif ext == 'pdf':
         ftype = 'application/pdf'
         # give user the filename...
         print 'Content-disposition: attachment; filename=%s' % base
      else: ftype = 'text/plain'

      f = open(os.path.join(course_dir,dir,fname),'rb')
      content = f.read()
      f.close()

      print 'Content-type: %s\n' % ftype
      print content
      return message
   except Exception,e:
      message = 'Internal error while reading file: %s' % e
   return message

# make a link to the most recent file meeting the specified criteria
def latest_file_link(student,assignment,staff=False):
   fname = '%s-%s-' % (student,assignment)
   files = glob.glob(os.path.join(course_dir,'files',fname + '*'))
   if len(files) > 0:
      files.sort()
      name = os.path.basename(files[-1])
      short_name = name[len(fname):]
      return '<A href="%s" target="_blank">%s</A>' %  (url_arg({'filename': name, 'action': 'View'}),short_name)
   else:
      return '&nbsp;'

# output form for entering/displaying grades for each assignment
def grade_form(student):
   global assignments,grades

   grades = grades_db.build_grades_dict(who=student)

   print '    <form action="grades.cgi" method="post">'
   print '      <input type="hidden" name="student" value="%s"/>' % student

   print '<table cellpadding="2" border="1" style="border-collapse:collapse;">'
   print '<tr><th>Assignment</th><th>Points</th><th>Checkoff</th><th>Staff<br>Extension</th><th>Actions</th></tr>'
   student_grades = grades.get(student,{})
   for a,tpts,fpct,issued,due in assignments:
      points,submit_time,checkoff_time,extension,action_list = get_grade_info(student_grades,a)

      if a[0] == 'Q':
         print '<tr><td>%s</td>' % a,
      else:
         print '<tr><td>%s<br><A href="pset.cgi?_assignment=%s&_as=%s">student view</A></td>' % (a,a,student)

      if points is None: points = ''
      print '<td><input type="text" name="%s points" size="5" value="%s"/>/%g</td>' % (a,points,tpts),

      if a[0] == 'Q':
         # quiz
         print '<td bgcolor="#E0E0E0">&nbsp;</td><td bgcolor="#E0E0E0">&nbsp;</td>'
      else:
         checked = ('checked','') if checkoff_time else ('','checked')
         print '<td><input type="radio" name="%s checkoff" value="yes" %s/>yes<input type="radio" name="%s checkoff" value="no" %s/>no</td>' % (a,checked[0],a,checked[1]),

         checked = ('checked','') if extension=='staff' else ('','checked')
         print '<td><input type="radio" name="%s extension" value="yes" %s/>yes<input type="radio" name="%s extension" value="no" %s/>no</td>' % (a,checked[0],a,checked[1]),

      print '<td>',
      for action,submitter,timestamp,value in action_list:
         if action == 'note': print '<font color="red">',
         print '%s (%s, %s): %s<br/>' % (action,submitter,display_timestamp(timestamp),value),
         if action == 'note': print '</font>',
      print 'Add note:<input type="text" name="%s notes" size="50" value=""/>' % a,
      print '</td>',

      print '</tr>'
   print '</table>'

   print '<p/><input type="submit" name="action" value="submit"/>'
   print '</form>'

# process grade submission
def grade_submission(args,requester):
   global grades,users,assignments
   student = args.getfirst('student')
   message = None

   if args.getfirst('action',None) == 'submit':
      grades = grades_db.build_grades_dict(who=student)
      student_grades = grades.get(student,{})

      # do an assignment-by-assignment diff to see what changed
      for a,tpts,fpct,issued,due in assignments:
         points,submit_time,checkoff_time,extension,action_list = get_grade_info(student_grades,a)
         for suffix in ("points","checkoff","extension","notes"):
            v = args.getfirst(a+" "+suffix,None)
            if v:
               # there's an entry in the incoming form
               if suffix == "notes":
                  if len(v) > 0:
                     # incoming notes are always added to the database
                     grades_db.add_action(student,a,'note',requester,v)
               elif suffix == "extension":
                  if v=='yes' and extension!='staff':
                     # a staff extension overrides previous extensions
                     grades_db.add_action(student,a,'extension',requester,'staff')
                  elif v=='no' and extension=='staff':
                     # only remove a staff-granted extension
                     grades_db.add_action(student,a,'extension',requester,None)
               elif suffix == "checkoff":
                  if v=='yes' and checkoff_time is None:
                     # checkoff completed
                     grades_db.add_action(student,a,'checkoff',requester,'yes')
                  elif v=='no' and checkoff_time:
                     # remove previous checkoff
                     grades_db.add_action(student,a,'checkoff',requester,None)
               elif suffix == "points":
                  try:
                     p = float(v)
                     if p >= 0 and p <= tpts:
                        if points != p:
                           # updated score
                           grades_db.add_action(student,a,'grade',requester,v)
                     else:
                        message = 'Points for %s (%g) must be >= 0 and <= %g. No changes made.' % (a,p,tpts)
                        break
                  except Exception,e:
                     message = 'Non-numeric %s for %s, %s points.  No changes made.' % (suffix,a,p)
                     break
            else:
               if suffix == "points" and points is not None:
                  # score removed
                  grades_db.add_action(student,a,'grade',requester,None)
         if message: break

   # output form for entering grades
   name = users.get(student,('???',None,None,None))[0]
   print '<h2>Grade entry for',name,'</h2>'
   if message: print '<p><font color="red">',message,'</font>'
   grade_form(student)
   print '<p><A href="%s">Summary Page</A>' % url_arg({})

# build grades summary from grades list.  Result is a list
# of dictionaries, one per student.
def build_summary():
   global grades,users
   grades = grades_db.build_grades_dict()

   # compute summary info
   summary = []
   for student in grades:
      student_grades = grades[student]

      sinfo = users.get(student,('???','???','???','???','???'))
      if sinfo[1] != 'student': continue
      name = sinfo[0]
      email = student
      ta = sinfo[3]
      info = {'student': name,
              'email': email,
              'ta': ta,
              'missing_checkoff': False,
              'pset_total': 0,
              'total': 0}
      for a,tpts,fpct,issued,due in assignments:
          points,submit_time,checkoff_time,extension,action_list = get_grade_info(student_grades,a)
          score,late = compute_score(a,tpts,due,points,submit_time,checkoff_time,extension)
          if a[0] == 'P' and not checkoff_time: info['missing_checkoff'] = True
          info[a] = score;
          if score:
             p = (float(score)/tpts)*fpct*100
             info['total'] += p
             if a[0] == 'P': info['pset_total'] += p
      summary.append(info)

   summary.sort(lambda x,y: cmp(x['student'],y['student']))
   return summary

# html grade summary
def html_grade_summary(args):
   summary = build_summary()

   # sort by requested key
   if args.has_key('sort'):
      sortfield = args['sort'].value
   else:
      sortfield = 'total'
   if sortfield == 'student' or sortfield == 'ta':
      summary.sort(lambda x,y: cmp(x[sortfield],y[sortfield]))
   else:
      summary.sort(lambda x,y: cmp(y[sortfield],x[sortfield]))
   
   print '<h2>%s Grade Summary</h2>' % CourseTerm

   statistics()

   # print stats for total scores
   n = 0
   psum = 0
   psumsq = 0
   psumDP = 0
   psumsqDP = 0
   histogram = {}
   for entry in summary:
      #if entry.get('Q1',None) == None or entry.get('Q2') == None: continue
      n += 1
      total = entry['total']
      psum += total
      psumsq += total*total
      total = int(total)
      histogram[total] = histogram.get(total,0) + 1
   print '<p>Stats for total score.<ul>'
   print 'number of students: %d' % n
   mean = psum/n if n > 0 else 0
   stddev = math.sqrt((psumsq - n*mean*mean)/(n-1)) if n > 0 else 0
   print '<br>avg=%0.2f, stddev=%0.2f' % (mean,stddev)
   print '</ul>'
   print '<p><table border="1" style="border-collapse:collapse;"><tr align="center"><td>Histogram of total score<br>',print_histogram(histogram,height=200),'</td></tr></table>'

   # print stats for total pset scores
   n = 0
   psum = 0
   psumsq = 0
   psumDP = 0
   psumsqDP = 0
   histogram = {}
   for entry in summary:
      n += 1
      total = entry['pset_total']
      psum += total
      psumsq += total*total
      total = int(total)
      histogram[total] = histogram.get(total,0) + 1
   print '<p>Stats for total PSET score.<ul>'
   print 'number of students: %d' % n
   mean = psum/n if n > 0 else 0
   stddev = math.sqrt((psumsq - n*mean*mean)/(n-1)) if n > 0 else 0
   print '<br>avg=%0.2f, stddev=%0.2f' % (mean,stddev)
   print '</ul>'
   print '<p><table border="1" style="border-collapse:collapse;"><tr align="center"><td>Histogram of total PSET score<br>',print_histogram(histogram,height=200),'</td></tr></table>'

   print '<p>Historically we\'ve given out about 40% A\'s and 40% B\'s in 6.02'

   print '<p>Missing lab scores marked in red.'

   # header row
   print '<p><table border="1" style="border-collapse:collapse;" cellpadding="2" bgcolor="white">'
   print '  <tr valign="top">'
   print '    <th> # </th>'
   print '    <th> <A href="%s">Student</A> </th>' % url_arg({'sort': 'student','html_grade_summary':'true'})
   print '    <th> Email </th>'
   print '    <th> <A href="%s">TA</A> </th>' % url_arg({'sort': 'ta','html_grade_summary':'true'})
   print '    <th> <A href="%s">Total</A> </th>' % url_arg({'sort': 'total','html_grade_summary':'true'})
   print '    <th> <A href="%s">PSet Total</A> </th>' % url_arg({'sort': 'pset_total','html_grade_summary':'true'})
   for a,tpts,fpct,issued,due in assignments:
      print '    <th> <A href="%s">%s</A><font size="-1"><br>%d<br>%.1f%%</font></th>' % (url_arg({'sort': a,'html_grade_summary':'true'}),a,tpts,fpct*100)
   print '    <th>Checkoffs</th>'
   print '  </tr>'

   row = 0
   for entry in summary:
      if entry['student'] == '???': continue
      if row % 2 == 0:
         print '  <tr bgcolor="#D0FFD0">'
      else:
         print '  <tr>'
      print '    <td><font color="grey">%d</td>' % (row+1)
      print '    <td>',entry['student'],'</td>'
      print '    <td>',entry['email'],'</td>'
      print '    <td>',entry['ta'],'</td>'
      print '    <td>',int(100*entry['total'])/100.0,'</td>'
      print '    <td>',int(100*entry['pset_total'])/100.0,'</td>'
      for a,tpts,fpct,issued,due in assignments:
         score = entry[a]
         if score is None:
            print '    <td>&nbsp;</td>'
         else:
            print '    <td align="right">',"%6.2f"%entry[a],'</td>'
      if entry['missing_checkoff']: print '    <td bgcolor="red">MISSING</td>'
      else: print '    <td>ok</td>'
      row = row + 1

   print '</table>'

def col2letters(col):
   if col < 26: return chr(ord('A') + col)
   else: return chr(ord('A') + int(col/26) - 1) + chr(ord('A') + (col % 26))

# colon separated values grade summary
def csv_grade_summary(args):
   summary = build_summary()
   fields = 'Student,Email,Total,PSet Total'
   total_formula = '='
   pset_total_formula = '='
   col = 4
   for a,tpts,fpct,issued,due in assignments:
      fields += ',%s' % a
      total_formula += '+(($%s%%(row)d/%g)*%g)' % (col2letters(col),tpts,100*fpct)
      if a[0] == 'P':
         pset_total_formula += '+(($%s%%(row)d/%g)*%g)' % (col2letters(col),tpts,100*fpct)
      col += 1
   fields += ',Checkoffs'
   print fields

   row = 1
   for entry in summary:
      row += 1
      data = '"%s","%s",%s,%s' % (entry['student'],entry['email'],\
                                  total_formula % {'row':row},\
                                  pset_total_formula % {'row':row})
      for a,tpts,fpct,issued,due in assignments:
         pts = entry[a] or 0
         data += ',%g' % pts
      data += ',"%s"' % ('MISSING' if entry['missing_checkoff'] else 'ok')
      print data

# output email addresses for staff, students
def email_list():
   global users
   print '<h2>Class email list</h2>'
   studlist = [(email,info[0]) for email,info in users.items() 
               if info[1] == 'student']
   studlist.sort(lambda x,y: cmp(x[1],y[1]))
   elist = ['"%s"&lt;%s&gt;' % (name,email) for (email,name) in studlist]
   print '<p>','<br>'.join(elist)

   print '<p><h2>LA email list</h2>'
   lalist = [(email,info[0]) for email,info in users.items() if info[1] == 'la']
   lalist.sort(lambda x,y: cmp(x[1],y[1]))
   elist = ['"%s"&lt;%s&gt;' % (name,email) for (email,name) in lalist]
   print '<p>','<br>'.join(elist)

   print '<p><h2>Grader email list</h2>'
   glist = [(email,info[0]) for email,info in users.items() if info[1] == 'grader']
   glist.sort(lambda x,y: cmp(x[1],y[1]))
   elist = ['"%s"&lt;%s&gt;' % (name,email) for (email,name) in glist]
   print '<p>','<br>'.join(elist)

   print '<p><h2>Staff email list</h2>'
   stafflist = [(email,info[0]) for email,info in users.items() if info[1] == 'staff']
   stafflist.sort(lambda x,y: cmp(x[1],y[1]))
   elist = ['"%s"&lt;%s&gt;' % (name,email) for (email,name) in stafflist]
   print '<p>','<br>'.join(elist)

   # email list for each Interviewer
   for semail,sname in stafflist:
      tname = sname.split(', ')
      slist = [(email,info[0]) for email,info in users.items() if info[1] == 'student' and info[3]==semail]
      if len(slist) > 0:
         slist.sort(lambda x,y: cmp(x[1],y[1]))
         print '<p><h2>Interview Email list for %s %s [%d]</h2>' % (tname[1],tname[0],len(slist))
         elist = ['"%s"&lt;%s&gt;' % (name,email) for (email,name) in slist]
         print '<p>','<br>'.join(elist)

   # email list for each Grader
   for gemail,gname in glist:
      tname = gname.split(', ')
      slist = [(email,info[0]) for email,info in users.items() if info[1] == 'student' and info[4]==gemail]
      if len(slist) > 0:
         slist.sort(lambda x,y: cmp(x[1],y[1]))
         print '<p><h2>Student list for %s %s [%d]</h2>' % (tname[1],tname[0],len(slist))
         elist = ['"%s"&lt;%s&gt;' % (name,email) for (email,name) in slist]
         print '<p>','<br>'.join(elist)

# histogram for assignment grades
def assignment_histogram(a):
   summary = build_summary()

   print '<h2>Histogram for ',a,'</h2>'
   histogram = {}
   for entry in summary:
      if entry.has_key(a):
         try:
            grade = int(entry[a])
         except Exception:
            continue
         histogram[grade] = histogram.get(grade,0) + 1
   print print_histogram(histogram)

# main staff page: status summary
def status_summary(role):
   print '<h2>%s Status Summary</h2>' % CourseTerm

   print '<p>Grade Summaries: '
   print '<A href="%s">HTML</A>' % url_arg({'html_grade_summary': 'true'})
   print ', <A href="%s">CSV</A>' % url_arg({'csv_grade_summary': 'true'})

   """
   print '<p>Histograms: '
   for a,tpts,fpct,deadline,tasks in assignments:
      if a[0] == 'Q':
         print '&nbsp;<A href="%s">%s</A>' % (url_arg({'histogram': a}),a)
   """

   print '<p><A href="%s">Email lists</A>' % url_arg({'email_list': 'true'})
   
   course_only_files()

   #print '<p><A href="%s">Student Pictures</A>' % url_arg({'action': 'View','filename': 'pictures'},cgi='pdf')

   # table showing grades for each student
   grades_table(role)

   statistics()

# process bulk entry request
def bulk_entry(requester,args):
   # locate assignment
   assignment = args.getfirst('assignment','???')
   for a,tpts,fpct,issued,due in assignments:
      if a == assignment: break
   else:
      print "Oops, can't find assignment %s" % assignment
      return

   grades = grades_db.build_grades_dict(assignment=assignment)
   slist = [(email,info[0])
            for email,info in users.items() if info[1]=='student']
   slist.sort(lambda x,y: cmp(x[1],y[1]))

   # handle grade submission
   if args.getfirst('bulk_entry',None) == 'Submit Grades':
      for (email,name) in slist:
         points = grades.get(email,None)
         if points: points = points.get(assignment,None)
         if points: points = points[0]

         p = args.getfirst(email,None)
         if p is not None:
            # update score, add to database
            if str(points) != p:
               grades_db.add_action(email,assignment,'grade',requester,str(p))
         else:
            # score removed
            if points is not None:
               grades_db.add_action(email,assignment,'grade',requester,None)

      # rebuild grades table to reflect changes
      grades = grades_db.build_grades_dict(assignment=assignment)

   print '<b>Bulk grade entry for %s</b><p>' % assignment

   print '<form action="grades.cgi" method="post">'
   print '<input type="hidden" name="assignment" value="%s">' % assignment
   print '<p><table border="1" cellpadding="2" style="border-collapse:collapse;">'
   print '<tr><th>Student</th><th>Points</th></tr>'

   for (email,name) in slist:
      points = grades.get(email,None)
      if points: points = points.get(assignment,None)
      if points: points = points[0]

      if points is None: points = ''
      print '<tr><td>%s</td>' % name
      print '<td><input type="text" size="5" name="%s" value="%s"></td>' % \
            (email,points)
      print '</tr>'

   print '</table>'
   print '<p><input type="submit" name="bulk_entry" value="Submit Grades">'
   print '</form>'

# process request from a staff member
def staff_request(requester,role,args):
   global grades

   if args.has_key('action') and args['action'].value == 'View':
      send_text_file(args['filename'].value)
      return

   if args.has_key('action') and args['action'].value == 'ViewCourse':
      send_text_file(args['filename'].value,dir='course_only')
      return

   if role=='staff' and args.has_key('csv_grade_summary'):
      print 'Content-type: text/plain\n'
      csv_grade_summary(args)
      return

   print 'Content-type: text/html\n'
   print '<html><body style="background-color:#FFFFD0;">'

   #print '<pre>'
   #for k in os.environ.keys():
   #   print k,os.environ.get(k)
   #print '</pre>'

   if role=='staff' and args.has_key('html_grade_summary'):
      html_grade_summary(args)
   elif args.has_key('bulk_entry'):
      bulk_entry(requester,args)
   elif args.has_key('histogram'):
      assignment_histogram(args['histogram'].value)
   elif args.has_key('email_list'):
      email_list()
   elif args.has_key('student'):
      grade_submission(args,requester)
   else:
      status_summary(role)

   print '</body></html>'

# process request from a grader
def grader_request(requester,args):
   global users,grades

   if args.has_key('action') and args['action'].value == 'View':
      send_text_file(args['filename'].value)
      return

   grades = grades_db.build_grades_dict()

   print 'Content-type: text/html\n'
   print '<html><body>'
   print '<h3>%s<p>PSet Grading: %s</h3>' % (CourseTerm,users[requester][0])
   print """<p><div style="width:600px">
   <p>To grade a submitted pset, click on the entry in appropriate row and column.
   Some students may submit their psets late, so please check earlier assignments
   to see if there are new psets to be graded.
   </div>
   """

   print '<p><table border="1" style="border-collapse:collapse;" cellpadding="2" bgcolor="white">'

   # header row
   print '  <tr>'
   print '    <th> # </th>'
   print '    <th> Student </th>'
   print '    <th> Email </th>'
   for a,tpts,fpct,issued,due in assignments:
      if a[0] != 'Q':
         print '    <th>%s</th>' % a
   print '  </tr>'

   # output table alphabetically by student name.  Select only those students
   # which have requester as their grader.
   slist = [(email,info[0])
            for email,info in users.items()
            if info[1]=='student' and info[4]==requester]
   slist.sort(lambda x,y: cmp(x[1],y[1]))
   row = 0
   for (email,name) in slist:
      bgcolor = '#D0FFD0' if row % 2 == 0 else '#FFFFFF'
      print '  <tr bgcolor="%s">' % bgcolor
      # student name and email
      print '    <td><font color="grey">%d</font></td>' % (row+1)
      print '    <td>',name,'</td>'
      print '    <td>',email,'</td>'
      student_grades = grades.get(email,{})
      for a,tpts,fpct,issued,due in assignments:
         if a[0] != 'Q':
            points,submit_time,checkoff_time,extension,action_list = get_grade_info(student_grades,a)
            score,late = compute_score(a,tpts,due,points,submit_time,checkoff_time,extension)
            score = '%4.2f' % score if score else 'grade' if submit_time else '&nbsp;'
            if submit_time:
               print '    <td align="right"><A href="pset.cgi?_assignment=%s&_student=%s">%s</A></td>' % (a,email,score)
            else:
               print '    <td>&nbsp;</td>'
      print '  </tr>'
      row = row + 1

   print '</table></body></html>'   

# process request from a student: print table of grades
def student_request(requester,args,staff=False):
   # process extension-related requests from student
   if args.has_key('extension'):
      assignment = args['assignment'].value
      if args['extension'].value == 'Use extension':
         grades_db.add_action(requester,assignment,'extension',requester,'student')
      elif args['extension'].value == 'Cancel extension':
         grades_db.add_action(requester,assignment,'extension',requester,None)

   student = users[requester][0]
   ta = users[requester][3]
   if users.has_key(ta):
      ta_name = users[ta][0].split(', ')
   else: ta_name = ('','')

   # student request to view submitted file
   if args.has_key('action') and args['action'].value == 'View':
      filename = args['filename'].value
      # only student's own files in response to student view request
      prefix = requester.split('@')[0]
      if filename.startswith(prefix): send_text_file(filename)
      return

   # student request to view course-only file
   if args.has_key('action') and args['action'].value == 'ViewCourse':
      send_text_file(args['filename'].value,dir='course_only')
      return

   grades = grades_db.build_grades_dict(who=requester)
   print 'Content-type: text/html\n'

   print '<html><body style="width:600px">'
   print '<h3>%s</h3><h3>Status for ' % CourseTerm,student,'</h3>'

   print '<p>Your interviewer is %s %s (%s).' % (ta_name[1],ta_name[0],ta)

   course_only_files()

   print '<p style="background-color: #CCEEFF;padding: 10px"><b>Points Summary</b>'

   print '<p>You can request an extension for up to two assignments by clicking'
   print 'on the appropriate buttons below (you can change your mind later and cancel'
   print 'an extension).  All extensions must be resolved by the last day of classes.'

   print '<p>Assignments with a late penalty are shown with a colored background.'

   print '<p><table border="1" style="border-collapse:collapse;" cellpadding="2" bgcolor="white">'
   # header row: assignment names
   print '  <tr><th>Assignment</th><th>Points</th><th>Checkoff</th><th>Extension</th>'
   student_grades = grades.get(requester,{})
   nextensions = count_extensions(student_grades)
   total = 0
   maxpts = 0
   # one row per assignment
   for a,tpts,fpct,issued,due in assignments:
      points,submit_time,checkoff_time,extension,action_list = get_grade_info(student_grades,a)
      score,late = compute_score(a,tpts,due,points,submit_time,checkoff_time,extension)

      maxpts += fpct*100
      # don't reveal quiz scores until after issued date
      if (score is None) or (a[0]=='Q' and now() < issued): score = '--'
      else: total += (score/tpts)*fpct*100

      if a[0] == 'Q':
         # quiz
         print '<tr><td align="center">%s</td><td>%s/%g</td><td bgcolor="#E0E0E0">&nbsp;<td bgcolor="#E0E0E0">&nbsp;</td></tr>' % (a,score,tpts)
      else:
         # pset
         print '<tr><td align="center">%s</td>' % a,
         bgcolor = '#FFA07A' if late else '#FFFFFF'
         print '<td bgcolor="%s">%s/%g</td>' % (bgcolor,score,tpts),
         # checkoff
         print '<td align="center">%s</td>' % ('&radic;' if checkoff_time else '&nbsp;'),
         # extension
         print '<td>',
         if extension == 'staff':
            print 'staff-granted extension'
         elif extension == 'student':
            print '<form style="margin-bottom:0px;display:inline;" action="grades.cgi" method="post">',
            if staff:
               print '<input type="hidden" name="as" value="%s"/>' % requester,
            print '<input type="hidden" name="assignment" value="%s"/>' % a,
            print '<input type="submit" name="extension" value="Cancel extension"/>',
            print '</form>'
         elif nextensions < 2:
            # only two student-requested extensions are possible
            print '<form style="margin-bottom:0px;display:inline;" action="grades.cgi" method="post">',
            if staff:
               print '<input type="hidden" name="as" value="%s"/>' % requester,
            print '<input type="hidden" name="assignment" value="%s"/>' % a,
            print '<input type="submit" name="extension" value="Use extension"/>',
            print '</form>'
         print '</td>',
         print '</tr>'
   print '</table>'
   print '<p>You must complete any missing pset checkoffs'
   print 'before 5p on the last day of classes.  A missing checkoff'
   print 'will result in a failing grade for the course.'
   print '<p>Total points earned to date (weighted by assignment, including all posted psets): %3.2f/%3.2f' % (total,maxpts)
   print '</ul>'

   print '</body></html>'

if __name__ == "__main__":
   # comment out the following line for normal operation
   print 'Content-type: text/plain\n\nSorry, the on-line grades system is not available after the end of term.'; sys.exit(0)

   import cgitb
   cgitb.enable()

   from xconfig import *

   name = os.environ.get('SSL_CLIENT_S_DN_CN','???')
   requester = os.environ.get('SSL_CLIENT_S_DN_Email','???')

   if users.has_key(requester):
      role = users[requester][1]

      args = cgi.FieldStorage()   # handle either POST or GET forms

      if role=='staff':
         cgitb.enable()
         if args.has_key('as'):
            s = args.getfirst('as','???')
            if users.has_key(s):
               if users[s][1] == 'grader':
                  grader_request(s,args)
               else:
                  student_request(s,args,staff=True)
               sys.exit(0)
         staff_request(requester,role,args)
      elif role=='grader':
         grader_request(requester,args)
      else:
         student_request(requester,args)
   else:
      print 'Content-type: text/html\n'
      print """<html><body>
      Sorry, but you are not listed as a staff member or student in 6.02.

      <p>If you've clicked cancel when the browser asks which
      certificate to use, it caches the answer and never asks again.
      If you're using Firefox, try going to Preferences -&gt; Advanced -&gt;
      Encryption and choose "Select one automatically" -- that usually fixes
      the problem.

      <p>If that's not the issue, please contact 6.02-web@mit.edu if you
      need access to this webpage and include the following two lines:
      <pre>
      SSL_CLIENT_S_DN_CN: %s
      SSL_CLIENT_S_DN_Email: %s</pre>
      </body></html>""" % (name,requester)
