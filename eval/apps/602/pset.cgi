#!/usr/bin/env python
# cjt - Jan 2011

import cgi,os,re,sys,time
import xgrades_db as grades_db
import pset_db

"""
Allow users to browse .pset files, process submittals.  URL:

pset.cgi?_assignment=foo

which will process the file psets/foo.pset.

A .pset file is a vanilla HTML file that is processed by this program,
which rewrites certain tags.  Here are the tags that get rewritten:

In the following tags, the value id property if required should be
unique within this file.  It is used to tag submitted answers in the
response database.  id's should not begin with _.

<include file="..."/>
  insert the contents of the specified file at this point.

<image src="..."/>
  This will get rewritten as a download request to pset.cgi.

<link href="..." ...>...</link>
  rewritten as <a> tag with appropriate href attribute

<answer id="uniqueid" points="maxpoints">solution</answer>
  When rendered for a student, a textarea input widget is displayed,
  initialized with the most recent submission by the student (if any).

  Points specifies the maximum point value for this answer.

  After the due date, the solution text is displayed below the
  student's submission.  Staff also see a field for entering points
  and comments.

<submit_file id="uniqueid" points="maxpoints" prompt="..."/>solution</submit_file>
  Prompt the user to submit a file, which is stored in the files
  subdirectory with under the name email-assignment-id-timestamp.ext
  where the extension is the same as that of the filename on the
  user's file system.  If files have been submitted, links are
  provided for viewing the files in a separate window.

  Points specifies the maximum point value for this answer.

  After the due date, the solution text is displayed below the
  student's submission.  Staff also see a field for entering points
  and comments.

"""

##################################################
#
# Cascading Style Sheet for psets
#
##################################################

scripts_and_css = """
<script type="text/javascript" src="https://web.mit.edu/6.02/www/currentsemester/handouts/prettify.js"></script>
<script type="text/javascript">
function confirm_submit() {
  msg = "Are you absolutely sure that you want to submit this assignment? YOU CAN SUBMIT ONLY ONCE after which you will not be able to make any further changes to your answers.";
  return confirm(msg);
}
</script>
<style type="text/css">
body {
  width:600px;
  line-height:1.25;
}

.instructions {
  background: #eeeeff;
  margin-bottom: 20px;
  padding-left: 10px;
  padding-right: 10px;
  padding-top: 10px;
  border: double;
  border-width: normal;
}

.code {
  background: #ffffe0; 
  line-height: 1.0;
  margin-left: 20px;
  margin-bottom: 20px;
  padding-left: 20px;
  padding-right: 20px;
  padding-bottom: 15px;
  border: dashed;
  border-width: thin;
}

.upload {
  padding: 5px;
  background-color: #cceeff;
  border: 1px solid #888;
}

textarea#answer {
  width: 100%;
  height: 100px;
  max-width: 100%;
  padding: 5px;
  background-color: #cceeff;
  border: 1px solid #888;
  resize: vertical;
  overflow: auto;
}

textarea#comments {
  width: 100%;
  height: 100px;
  max-width: 100%;
  padding: 5px;
  border: 1px solid #888;
  resize: vertical;
  overflow: auto;
}

/* Pretty printing styles. Used with prettify.js. */

.str { color: #0A0; }
.kwd { color: #00C; }
.com { color: #888; }
.typ { color: #606; }
.lit { color: #066; }
.pun { color: #660; }
.pln { color: #000; }
.tag { color: #008; }
.atn { color: #606; }
.atv { color: #080; }
.dec { color: #606; }

pre.prettyprint {
  background: #fffff0; 
  line-height: 1.25;
  margin-top: 20px;
  margin-left: 20px;
  margin-bottom: 20px;
  padding-left: 20px;
  padding-right: 20px;
  padding-bottom: 15px;
  padding-top: 15px;
  border: dashed;
  border-width: thin;
 }

@media print {
  .str { color: #060; }
  .kwd { color: #006; font-weight: bold; }
  .com { color: #600; font-style: italic; }
  .typ { color: #404; font-weight: bold; }
  .lit { color: #044; }
  .pun { color: #440; }
  .pln { color: #000; }
  .tag { color: #006; font-weight: bold; }
  .atn { color: #404; }
  .atv { color: #060; }
}
</style>
"""

##################################################
#
# Tag classes for handling special pset tags
#
##################################################

# base class for special pset tags
class pset_tag(object):
   def __init__(self,attrs,content):
      self.attributes = {}
      self.content = [content] if content else None  # single string in a list

      # parse attr string, filling the attribute dictionary.
      # An attribute has form
      #  name
      #  name=value
      #  name="..."
      #  name='...'
      # The pattern below is meant to be used with re.findall or re.finditer
      pattern = r'''\s*(?P<name>\w+)(\s*=\s*(("(?P<value1>.*?)")|('(?P<value2>.*?)')|(?P<value3>\w+)))?'''

      for a in re.finditer(pattern,attrs):
         name = a.group('name')
         value = a.group('value1') or a.group('value2') or a.group('value3') or True
         self.attributes[name] = value

   # return a string describing the attributes, leaving
   # out attributes whose name appears in excluded
   def other_attributes(self,excluded=()):
      return ' '.join(['%s="%s"' % (n,v)
                       for n,v in self.attributes.items()
                       if not (n in excluded)])

   def tag_name(self): return '???'

   def parse_tag(self,tag,constructor):
      # look in our content for tags that need parsing
      if self.content:
         self.content = parse_tag(self.content,tag,constructor)

   def content_html(self,assignment,answers,grading,solutions):
      result = ""
      if self.content:
         for c in self.content:
            if isinstance(c,pset_tag):
               result += c.html(assignment,answers,grading,solutions)
            else:
               result += str(c)
      return result

   # override to generate html for this tag
   def html(self,assignment,answers,grading=False,solutions=False,question_only=False):
      result = '&lt;%s' % self.tag_name()
      for n,v in self.attributes.items():
         result += ' %s="%s"' % (n,v)
      if self.content:
         result += '&gt;%s&lt;/%s&gt;' % (self.content_html(assignment,answers,grading,solutions),self.tag_name())
      else:
         result += '/&gt;'
      return result

   # return HTML for viewing grading and solutions
   def display_grading(self,id,assignment,answers,grading,solutions):
      result = ''
      id_comments = id + '_comments'
      comments = answers.get(id_comments,'')
      id_points = id + '_points'
      points = answers.get(id_points,'')

      max_points = self.attributes.get('points','???')

      if grading:
         # display points/comments fields if grading
         result += \
         '<p><table bgcolor="#ffccee" border="1" style="border-collapse:collapse;" width="100%%">' \
         '<tr align="left"><th>score:</th><td width="100%%"><input type="text" size="5" name="%s" value="%s"/> (max points: %s)</td></tr>' \
         '<tr align="left"><th>comments:</th><td><textarea id="comments" name="%s">%s</textarea></td></tr>' \
         '</table>' % (id_points,points,max_points,id_comments,html_encode(comments))
      elif len(points) > 0 or len(comments) > 0:
         # otherwise if there are points and/or comments show them to user
         result += '<p><font color="red"><b>score:</b> %s</font><br>' % points
         if len(comments) > 0:
            result += '<font color="red"><b>comments:</b> %s</font><br>' % comments

      # display solutions
      if solutions and self.content:
         result += '\n<p><font color="blue">Solution: '+self.content_html(assignment,answers,grading,solutions)+'</font>\n'

      return result

# <image src="..." .../>
# rewritten as <img> tag with appropriate src field to access otherwise
# inaccessbile pset directory.  Note that image file name must begin
# with PSx_ where PSx is the name of the assignment.
class image_tag(pset_tag):
   def tag_name(self): return 'image'

   def html(self,assignment,answers,grading=False,solutions=False,question_only=False):
      fname = self.attributes.get('src','???')
      prefix =  assignment+'_'
      assert len(fname) > len(prefix) and fname[:len(prefix)] == prefix,\
             "image name doesn't start with assignment name"
      # pass along any specified attributes to the html <img> tag
      attrs = self.other_attributes(excluded=('src',))
      return '<img src="pset.cgi?_assignment=%s&_download=%s" %s>' % \
             (assignment,fname[len(prefix):],attrs)

# <link href="..." ...>...</link>
# rewritten as <a> tag with appropriate src field to access otherwise
# inaccessbile pset directory.  Note that the file name must begin
# with PSx_ where PSx is the name of the assignment.
class link_tag(pset_tag):
   def tag_name(self): return 'link'

   def html(self,assignment,answers,grading=False,solutions=False,question_only=False):
      fname = self.attributes.get('href','???')
      prefix =  assignment+'_'
      assert len(fname) > len(prefix) and fname[:len(prefix)] == prefix,\
             "file name doesn't start with assignment name"
      # pass along any specified attributes to the html <a> tag
      attrs = self.other_attributes(excluded=('href',))
      return '<a href="pset.cgi?_assignment=%s&_download=%s" %s>%s</a>' % \
             (assignment,fname[len(prefix):],attrs,self.content_html(assignment,answers,grading,solutions))

# <include file="..."/>
# insert the contents of the specifed file in place of the tag
class include_tag(pset_tag):
   def tag_name(self): return 'include'

   def html(self,assignment,answers,grading=False,solutions=False,question_only=False):
      fname = os.path.join(pset_path,self.attributes.get('file','???'))
      assert os.path.exists(fname),"can't find file "+fname
      f = open(fname,'rb')
      content = f.read()
      f.close()
      return content

# <answer id="unique_id" points="max_points">solution...</answer>
# When rendered for a student, a textarea input widget is displayed,
# initialized with the most recent submission by the student (if any).
# Points specifies the maximum point value for this answer.
# After the due date, the solution text is displayed below the
# student's submission.  Staff also see a field for entering points
# and comments.
class answer_tag(pset_tag):
   def tag_name(self): return 'answer'

   def html(self,assignment,answers,grading=False,solutions=False,question_only=False):
      if question_only: return ''

      id = self.attributes.get('id',None)
      assert id is not None,"missing id attribute"
      answer = answers.get(id,'')

      # display user's answer
      disabled = 'disabled' if solutions and not grading else ''
      result = '<textarea id="answer" name="%s" %s>%s</textarea>' % \
               (id,disabled,html_encode(answer))

      # display point value
      points = self.attributes.get('points',None)
      if points:
         result += '(points: %s)<br>' % points

      # followed by grading info and solutions
      result += self.display_grading(id,assignment,answers,grading,solutions)

      return result

#<submit_file id="uniqueid" points="maxpoints" prompt="...">solution</submit_file>
# Prompt the user to submit a file, which is stored in the files
# subdirectory with under the name email-assignment-id-timestamp.ext
# where the extension is the same as that of the filename on the
# user's file system.  If files have been submitted, links are
# provided for viewing the files in a separate window.
# Points specifies the maximum point value for this answer.
# After the due date, the solution text is displayed below the
# student's submission.  Staff also see a field for entering points
# and comments.
class submit_file_tag(pset_tag):
   def tag_name(self): return 'submit_file'

   def html(self,assignment,answers,grading=False,solutions=False,question_only=False):
      if question_only: return ''

      id = self.attributes.get('id',None)
      assert id is not None,"missing id attribute"
      answer = answers.get(id,'')
      prompt = self.attributes.get('prompt','Please specify a file')

      result = '<div class="upload">'
      # first arrange to remember current file name and let user access file
      if len(answer) > 0:
         result += '<input type="hidden" name="%s" value="%s">' % (id,answer)
         result += '<A href="grades.cgi?action=View&filename=%s" target="_blank">View uploaded file</A>' % answer
      # now add a way for user to upload a new file
      disabled = 'disabled' if solutions and not grading else ''
      result += '<p>%s <input type="file" name="%s_file" value="" size="40" %s>' % (prompt,id,disabled)
      result += '</div>'

      # display point value
      points = self.attributes.get('points',None)
      if points:
         result += '(points: %s)<br>' % points

      # followed by grading info and solutions
      result += self.display_grading(id,assignment,answers,grading,solutions)

      return result

##################################################
#
# Helper functions for interacting with user
#
##################################################

# send file to browser; return error message, if any
def send_file(fname,dir='psets'):
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
      elif ext == 'wav': ftype = 'audio/x-wav'
      elif ext == 'pdf': ftype = 'application/pdf'
      elif ext == 'py': ftype = 'text/plain'
      else: ftype = 'text/plain'

      f = open(os.path.join(course_dir,dir,fname),'rb')
      content = f.read()
      f.close()

      print 'Content-disposition: attachment; filename=%s' % base
      print 'Content-type: %s\n' % ftype
      print content,
      return message
   except Exception,e:
      message = 'Internal error while reading file: %s' % e
   return message

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

# rewrite < and & in text so that they display correctly in browser
def html_encode(text):
   text = text.replace('&','&amp;')
   text = text.replace('<','&lt;')
   return text

##################################################
#
# Parsing and rendering of a .pset file
#
##################################################

def parse_pset(assignment):
   # read in the pset
   f = open(os.path.join(pset_path,assignment+'.pset'),'r')
   pset = [f.read()]   # list with a single string
   f.close()
   return parse(pset)

# returns list of tag instances and strings
def parse(pset):
   # look for instances of <tag ... /> or <tag ...>...</tag>
   # and create appropriate pset_tag instances.  When parsing
   # is complete pset should be a list of strings and instances
   # in document order.
   for tag,constructor in (# container elements 
                           ('answer',answer_tag),
                           ('submit_file',submit_file_tag),
                           # leaf elements 
                           ('include',include_tag),
                           ('image',image_tag),
                           ('link',link_tag),
                           ):
      result = parse_tag(pset,tag,constructor)
      # use the newly parsed result
      pset = result

   # all done...
   return pset

def parse_tag(pset,tag,constructor):
   result = []
   tag_open = '<' + tag
   tag_close = '</' + tag + '>'
   # process the content list element-by-element looking
   # for tag instances.
   for element in pset:
      if isinstance(element,str):
         start = 0
         while True:
            # look for start of tag
            index = element.find(tag_open,start)
            if index == -1:
               if start < len(element):
                  # output any remaining text
                  result.append(element[start:])
               break
            # find end of tag
            end = element.find('>',index)
            if end == -1: break   # oops, malformed
            if element[end - 1] == '/':
               # tag of the form <tag ... />, so no content
               attrs = element[index + len(tag_open):end - 1]
               content = ''
               next_start = end + 1
            else:
               # tag of the form <tag ... >...</tag>
               attrs = element[index + len(tag_open):end]
               end2 = element.find(tag_close,end + 1)
               if end2 == -1:  # oops, assume user meant to say <tag ... />
                  content = ''
                  next_start = end + 1
               else:
                  content = element[end+1:end2]
                  next_start = end2 + len(tag_close)
            # first some text before start of tag
            if index > start: result.append(element[start:index])
            # now an instance for the tag itself
            result.append(constructor(attrs,content))
            start = next_start
      else:
         element.parse_tag(tag,constructor)
         result.append(element)
   return result

# just display the questions in the pset, no interaction fields, etc.
def show_pset(CourseTerm,path):
    global course_dir,pset_path
    course_dir = path
    pset_path = os.path.join(path,'psets')
    args = cgi.FieldStorage()   # handle either POST or GET forms

    # figure out which assignment is being accessed
    assignment = args.getfirst('_assignment','???')
    if not os.path.exists(os.path.join(pset_path,assignment+'.pset')):
        print 'Content-type: text/html\n'
        print "<html><body>Sorry, but %s is not valid pset name.</body></html>" % assignment
        sys.exit(0)

    # deal with download requests
    if args.has_key('_download'):
        msg = send_file(os.path.join(pset_path,assignment+'_'+args.getfirst('_download')))
        if msg:
            print 'Content-type: text/plain\n'
            print 'Oops, an internal error has occurred:\n%s\n' % msg
        sys.exit(0)

    pset = parse_pset(assignment)

    print 'Content-type: text/html\n'
    print '<html>'
    print '<head>'
    print '<meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">'
    print '<title>%s: %s</title>' % (CourseTerm,assignment)
    print scripts_and_css
    print '</head>'
    print '<body onload="prettyPrint()">'

    print '<h2>%s PSet %s</h2>' % (CourseTerm,assignment)

    # render each content element.  Use sys.stdout to avoid inserting
    # any spaces in case that would affect the semantics...
    for element in pset:
        if isinstance(element,str):
            sys.stdout.write(element)
        else:
            # rewrite the special tags into html for the user.  Result
            # depends on who's asking and the current interaction state
            # as captured in the answers dictionary.
            sys.stdout.write(element.html(assignment,{},question_only=True))

    print '</body></html>'

# pset is a list of strings and pset_tag instances
# role is one of 'staff', 'grader', 'student'
# assignment is a string giving the assignment name, eg, 'PS1"
# answers is a dictionary mapping field ids to user response strings
# requester is email of who's visiting the page
# student is email of who's pset is being examine
# args is the cgi.FieldStorage data structure from the incoming request
def render_pset(pset,role,assignment,answers,requester,student,args):
   global assignments

   # read in any grading info for this (student,assignment)
   grades = grades_db.build_grades_dict(who=student,assignment=assignment)
   sdict = grades.get(student,{})
   points,submit_time,checkoff_time,extension,action_list = \
      sdict.get(assignment,[None,None,None,None,[]])
   
   # find issued and due timestamps
   for a,tpts,fpct,issued,due in assignments:
      if a == assignment:
         pset_issued = issued
         pset_due = due
         break
   else:
         pset_issued = '99990101000000'
         pset_due = '99990101000000'
   now = time.strftime('%Y%m%d%H%M%S')  # current timestamp
   checkoff_due = increment_timestamp(pset_due,5)

   # figure out which type of form to create: staff visiting a student
   # get a grading form, otherwise if the assignment hasn't been
   # submitted, create a save/submit form
   if (role == 'staff' or role == 'grader') and requester != student:
      # staff is viewing a student's page
      if submit_time:
         submit = 'grade'    # 'Submit Grades' button
         grading = True
      else:
         submit = None
         grading = False
      solutions = True
   elif submit_time is None:
      # user hasn't yet sumbitted
      submit = 'submit'   # 'Save' and 'Submit' buttons
      # staff can choose to grade their own solutions
      grading = False if role != 'staff' else args.getfirst('_grading',False)
      # staff/las can choose to see solutions before submission
      solutions = False if not (role == 'staff' or role == 'la') else args.getfirst('_solutions',False)
   else:
      # user has submitted
      submit = None
      grading = False
      solutions = role=='staff' or role=='la' or now > pset_due

   # render the pset
   print 'Content-type: text/html\n'
   print '<html>'
   print '<head>'
   print '<meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">'
   print '<title>%s: %s</title>' % (CourseTerm,assignment)
   print scripts_and_css
   print '</head>'
   print '<body onload="prettyPrint()">'

   # only display pset to students after the issued date
   if role != 'staff' and now < issued:
      print 'Sorry, this pset is not yet available.  Please come back after %s.' % \
            display_timestamp(issued)
      sys.exit(0)

   sinfo = users[student]
   rinfo = users[requester]

   # check for and report any duplicate field ids
   ids = []
   duplicate_ids = []
   for element in pset:
      if isinstance(element,pset_tag):
         id = element.attributes.get('id',None)
         if id:
            if id in ids:
               if id not in duplicate_ids:
                  duplicate_ids.append(id)
            else:
               ids.append(id)
   if len(duplicate_ids) > 0 and role == 'staff':
      print '<p><font color="red" size="+2">Duplicate field IDs found: %s</font><p>' % ", ".join(duplicate_ids)

   # start input form, if needed
   if submit:
      print '<form action="pset.cgi" method="POST" enctype="multipart/form-data">'
      print '<input type="hidden" name="_assignment" value="%s"/>' % assignment
      if submit == 'submit':
         print """<div class="instructions">
         To save your work, click the SAVE button at the bottom of this
         page.  You can revisit this page, revise your answers and SAVE
         as often as you like.

         <p>To submit the assignment, click the SUBMIT button at the
         bottom of this page.  YOU CAN SUBMIT ONLY ONCE.  Once the
         assignment has been submitted, you can continue to view this
         page but will no longer be able to make any changes to your
         answers.  </div>
         """
      elif submit == 'grade':
         print """<div class="instructions">
         When grading is complete, please click the SUBMIT GRADES
         button at the bottom of this page.  You can do this multiple
         times if your grading is split across multiple sessions.
         </div>
         """
   else:
         print """<div class="instructions">
         This assignment was submitted on %s.  After the due date, you
         will be able to view the solutions below.  After grading,
         points and grader's comments will also be available below.
         """ % display_timestamp(submit_time)
         if checkoff_time:
            print '<p>The checkoff meeting was completed on %s.' % \
                  display_timestamp(checkoff_time)
         else:
            print """
            <p>Remember to schedule the checkoff meeting with your interviewer before
            the checkoff deadline (%s).
            """ % display_timestamp(checkoff_due)
         print '</div>'

   print '<h3>%s: %s</h3>' % (CourseTerm,rinfo[0])
   if requester != student:
      print '<p><h3>Grading Problem Set %s for %s</h3><p>' % (assignment,sinfo[0])
   else:
      print '<p><h2>PSet %s</h2>' % assignment

   print '<p><b>Dates & Deadlines</b><ul><table>'
   print '<tr><td>issued:</td><td>%s</td></tr>' % display_timestamp(pset_issued)
   print '<tr><td>due:</td><td>%s</td></tr>' % display_timestamp(pset_due)
   print '<tr><td>checkoff due:</td><td>%s</td></tr>' % display_timestamp(checkoff_due)
   print '</table></ul>'

   print """
   <p>Help is available from the staff in the 6.02 lab (38-530) during
   lab hours -- for the staffing schedule please see the <A href="https://scripts.mit.edu:444/~6.02/currentsemester/hours.cgi">Lab Hours</A> page on the
   course website.  We recommend coming to the lab if you want help
   debugging your code.
   <p>For other questions, please try the 6.02 on-line Q&A forum at <A href="http://www.piazzza.com/class#602" target="_blank">Piazzza</A>.
    <p>Your answers will be graded by actual human beings, so your
   answers aren't limited to machine-gradable responses.  Some of the
   questions ask for explanations and it's always good to provide
   a short explanation of your answer.
   """

   # render each content element.  Use sys.stdout to avoid inserting
   # any spaces in case that would affect the semantics...
   for element in pset:
      if isinstance(element,str):
         sys.stdout.write(element)
      else:
         # rewrite the special tags into html for the user.  Result
         # depends on who's asking and the current interaction state
         # as captured in the answers dictionary.
         sys.stdout.write(element.html(assignment,answers,grading,solutions))

   # complete input form, if needed
   if submit == 'submit':
      # student submission
      print """
      <div class="instructions">
      You can save your work at any time by clicking the Save button
      below.  You can revisit this page, revise your answers and SAVE
      as often as you like.
      """
      print '<p><input type="submit" name="_submit" value="Save"/>'
      print """<p>To submit the assignment, click on the Submit
      button below.  YOU CAN SUBMIT ONLY ONCE after which you will
      not be able to make any further changes to your answers.
      Once an assignment is submitted, solutions will be visible
      after the due date and the graders will have access to your
      answers.  When the grading is complete, points and grader
      comments will be shown on this page.
      """
      print '<p><input type="submit" name="_submit" value="Submit" onclick="return confirm_submit()"/>'
      if role == 'staff' and args.has_key('_as'):
         # continue masquerading as a student
         print '<input type="hidden" name="_as" value="%s"/>' % student
      print '</div></form>'
   elif submit == 'grade':
      # staff submission
      print """
      <div class="instructions">
      You can submit the grading results
      (points and comments) by clicking the Submit Grades button
      below.  Multiple submissions are allowed.
      """
      print '<p><input type="submit" name="_submit" value="Submit Grades"/>'
      print '<input type="hidden" name="_student" value="%s"/>' % student
      print '</div></form>'


   print '</body></html>'

##################################################
#
# Request handling
#
##################################################

# process pset request
def process_request(role,assignment,requester,student,args):
   # deal with download requests
   if args.has_key('_download'):
      msg = send_file(os.path.join(pset_path,assignment+'_'+args.getfirst('_download')))
      if msg:
         print 'Content-type: text/plain\n'
         print 'Oops, an internal error has occurred:\n%s\n' % msg
      sys.exit(0)
   else:
      # parse the pset file
      pset = parse_pset(assignment)

      # set up db connection for pset state
      pdb = pset_db.pset_db(database,requester,student,assignment)

      # see if student has already submitted this assignment
      grades = grades_db.build_grades_dict(who=student,assignment=assignment)
      sdict = grades.get(student,{})
      submit_time = sdict.get(assignment,[None,None,None,None,[]])[1]

      submit = args.getfirst('_submit')
      if (submit=='Save' or submit=='Submit') and submit_time:
         # avoid accidental resubmissions caused by reloads
         submit = None

      # process any incoming answers
      if submit:
         # save as most recent state
         answers = {}
         # consume incoming fields with names not beginning with "_"
         for arg in args.keys():
            if arg[0] != '_':
               v = args.getfirst(arg)
               if len(v) > 0: answers[arg] = v
         # process any incoming files
         for fid in answers.keys():
            # if this field is an uploaded file, save it and remember filename
            if args[fid].filename:
               assert fid.endswith('_file'),"id of file field doesn't end with _file"
               id = fid[:-5]
               # extract file extension from user's name for the file
               ext = os.path.basename(args[fid].filename)
               ext_index = ext.rfind('.')
               ext = '' if ext_index == -1 else ext[ext_index:]

               # construct filename for saving uploaded file here on the server:
               # student-assignment-id-YYYYMMDD-HHMMSS.ext
               filename = '%s-%s-%s-%s%s' % (requester.split('@')[0],
                                             assignment,
                                             id,
                                             time.strftime('%Y%m%d-%H%M%S'),
                                             ext)

               # save the uploaded file
               try:
                  out = open(os.path.join(course_dir,'files',filename),'wb')
                  out.write(args[fid].value)
                  out.close()
                  # update previously remembered filename
                  answers[id] = filename
               except Exception,e:
                  pass
               # remove the file upload id from the answers dictionary
               del answers[fid]
         # add a new interaction to record updated state
         pdb.add_interaction(submit,answers)

         # make the appropriate entries in the grades database.
         if submit == 'Submit':
            grades_db.add_action(student,assignment,'submit',requester,None)
         elif (role == 'staff' or role == 'grader') and submit == 'Submit Grades':
            # add up all the assigned points
            points = 0
            for n,v in answers.items():
               # fields with names ending in _points are point values
               if n.endswith('_points'):
                  try:
                     points += float(v)
                  except Exception,e:
                     pass
            grades_db.add_action(student,assignment,'grade',requester,points)

         # redirect so that reloads won't repost the form
         uri = os.environ.get('REQUEST_URI')
         uri += '?_assignment=%s' % assignment
         for a in ('_as','_solutions','_grading'):
            if args.has_key(a): uri += '&%s=%s' % (a,args.getfirst(a))
         print 'Status: 303 See Other\nLocation: %s\n\n' % uri
         sys.exit(0)
      else:
         # no submission, look up last state in interactions database
         interactions = pdb.get_interactions()
         answers = pdb.get_interaction_items(interactions[0]) if len(interactions) > 0 else {}

      # render the pset for the user
      render_pset(pset,role,assignment,answers,requester,student,args)

if __name__ == '__main__':
   import cgitb

   # after end of term, simply display pset questions
   # comment out following line for normal operation
   show_pset('Spring 2011','/afs/athena.mit.edu/course/6/6.02/web_scripts/s2011/'); sys.exit(0)

   from xconfig import *

   name = os.environ.get('SSL_CLIENT_S_DN_CN','???')
   requester = os.environ.get('SSL_CLIENT_S_DN_Email','???')

   # vet the request
   if users.has_key(requester):
      role = users[requester][1]
      if role == 'staff': cgitb.enable()
      
      args = cgi.FieldStorage()   # handle either POST or GET forms

      # figure out which assignment is being accessed
      assignment = args.getfirst('_assignment','???')
      if not os.path.exists(os.path.join(pset_path,assignment+'.pset')):
         print 'Content-type: text/html\n'
         print "<html><body>Sorry, but %s is not valid pset name.</body></html>" % assignment
         sys.exit(0)

      # sort out who's asking and who's pset we're looking at
      student = requester
      if role=='staff' and args.has_key('_as'):
         # staff can masquerade as a student
         student = args.getfirst('_as')
         requester = student
      elif role=='staff' or role=='grader':
         if args.has_key('_student'):
            s = args.getfirst('_student','???')
            if users.has_key(s):
               # graders can only access their students
               if role=='staff' or users[s][4] == requester:
                  student = s
               else:
                  student = None
            else:
               student = None
            if student is None:
               print 'Content-type: text/html\n'
               print """<html><body>
               Sorry, but %s is not listed as a grader for %s.
               <p>Please contact 6.02-web@mit.edu if you need access to this webpage;
               send along your Athena username, MIT ID and the name of the student
               whose pset you were trying to access.
               </body></html>""" % (requester,s)
               sys.exit(0)
                  
      # everything checks out, so process the request
      process_request(role,assignment,requester,student,args)

   else:
      print 'Content-type: text/html\n'
      print """<html><body>
      Sorry, but you are not listed as a staff member or student in 6.02.

      <p>If you've clicked cancel when the browser asks which
      certificate to use, it caches the answer and never asks again.
      If you're using Firefox, try going to Preferences -&gt; Advanced -&gt;
      Encryption and choose "Select one automatically" -- that usually fixes
      the problem.

      <p>If that's not the issue, please contact 6.02-web@mit.edu if
      you need access to this webpage and include the following two
      lines:
      <pre> SSL_CLIENT_S_DN_CN: %s
      SSL_CLIENT_S_DN_Email: %s</pre>
      </body></html>""" % (name,requester)
