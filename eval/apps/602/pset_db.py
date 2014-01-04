import hashlib,os,pickle,time
import MySQLdb

db_schemas = [
"""create table if not exists interactions (
  interaction_id integer auto_increment primary key,
  username varchar(32) not null,
  assignment char(4) not null,
  action char(16) not null,
  submitter varchar(32),
  timestamp char(14),
  index username_assignment_index (username,assignment)
);""",

"""create table if not exists interaction_items (
  interaction_id integer references interactions,
  vname text,
  value text,
  index interaction_id_index (interaction_id)
);""",
]

# encapsulate all the database interactions
class pset_db(object):
    def __init__(self,database,requester,student,assignment):
        host,user,passwd,db = database
        self.connection = MySQLdb.connect(host=host,user=user,passwd=passwd,db=db)
        self.cursor = self.connection.cursor()
        
        # create the tables and indicies if we've just created the database
        for statement in db_schemas:
            self.cursor.execute(statement)

        self.username = student
        self.assignment = assignment
        self.requester = requester

    def close(self):
        self.connection.close()

    # commit any pending transaction
    def commit(self):
        self.connection.commit()

    # execute a query, return all results
    def execute(self,query,data):
        self.cursor.execute(query,data)
        return self.cursor.fetchall()

    # return list of (interaction_id, action), sorted by timestamp (most recent first)
    def get_interactions(self):
        self.cursor.execute(("select interaction_id from interactions where username=%s and assignment=%s" +
                             " order by timestamp desc"),
                            (self.username,self.assignment))
        return [v[0] for v in self.cursor.fetchall()]

    # return dictionary built from (key,value) pairs for a particular interaction
    def get_interaction_items(self,interaction_id):
        self.cursor.execute("select vname, value from interaction_items where interaction_items.interaction_id=%s",
                            (interaction_id,))
        dict = {}
        for key,value in self.cursor.fetchall():
            dict[key] = value
        return dict

    # return dictionary built from (key,value) pairs for all interactions.
    # each entry is a list of values (or None), most recent first.
    def get_all_interaction_items(self):
        interactions = self.getInteractions()

        answers = {}
        if len(interactions) > 0:
            # loop through interactions for this (user,doc), most recent first
            i = 0
            for id in interactions:
                # get interaction items for this particular interaction
                interaction_items = self.getInteractionItems(id)
                for key,val in interaction_items.items():
                    ans = answers.get(key)
                    if ans is None:
                        # new key, so add it to answers dictionary
                        ans = [None]*len(interactions)
                        answers[key] = ans
                    # fill in entry for this interaction item
                    ans[i] = val
                i = i + 1

        return answers

    # construct an insert statement from a dictionary, returns (statement, data tuple)
    def insert(self,table,dict,cmd='insert'):
        keys = dict.keys()
        names = ", ".join([n for n in keys])
        values = ", ".join(['%s' for n in keys])
        data = [dict[n] for n in keys]
        return (cmd+" into %s (%s) values (%s)" % (table,names,values), data)

    # construct an update statement from a dictionary, returns (statement, data tuple)
    def update(self,table,dict):
        keys = dict.keys()
        values = ", ".join(['%s=%%s' % n for n in keys])
        data = [dict[n] for n in keys]
        return ("update %s set %s" % (table,values),data)

    # add a new interaction to the database using info in the supplied dictionary,
    # returns interaction_id of the new entry
    def add_interaction(self,action,dict):
        data = {
            'username': self.username,
            'assignment': self.assignment,
            'action': action,
            'submitter': self.requester,
            'timestamp': time.strftime("%Y%m%d%H%M%S"),
            }

        # create a new row in the interactions table
        cmd,args = self.insert('interactions',data)
        self.cursor.execute(cmd,args)
        interaction_id = self.cursor.lastrowid
        data = {'interaction_id': interaction_id}

        # create a new row in the interaction_items table for each entry in dictionary
        for key in dict.keys():
            data['vname'] = key
            data['value'] = str(dict[key])
            cmd,args = self.insert('interaction_items',data)
            self.cursor.execute(cmd,args)

        # we're done with this transaction!
        self.connection.commit()

        # return interaction_id
        return interaction_id

