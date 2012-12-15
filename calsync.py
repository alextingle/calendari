#!/usr/bin/env python

import sys
import os
import os.path
import hashlib
import shutil
import urllib
import urllib2
import urlparse
import re
import time
import stat
from gi.repository import Notify
import string

CYCLE_TIME_SEC = 10
FULL_SYNC_MINS = 30
NETWORK_GRACE_MINS = 10

calsync_dir = os.path.join(os.environ['HOME'],'.calsync')
working_dir = os.path.join(calsync_dir,'.work')
config_file = os.path.join(calsync_dir,'config')

# Set of syn objects
_obj = []

# Index of filename -> sync objects
_idx = {}

# Queue of events: time -> pathname
_queue = {}

_log = None

S_OK = 0
S_NOCHANGE = 1
S_NOTFOUND = 2
S_TOOSOON = 3
_status = ['OK','NOCHANGE','NOTFOUND','TOOSOON']

global _next_full_sync
_next_full_sync = 0

global _last_seen
_last_seen = time.time()


def log(*args):
  _log.write(time.ctime(time.time())+'  ')
  _log.write(' '.join(map(str,args))+'\n')
  _log.flush()
  print ' '.join(map(str,args))


class SyncObj(object):
  def __init__(self,name,addr):
      self._name = name
      self._next = 0 # Time of next sync.
      fname = filter(lambda c: c not in ' /\\:.', name)
      self._addr = addr
      self._dir  = os.path.join(working_dir,fname + '.d')
      self._last = os.path.join(self._dir,'last')
      self._this = os.path.join(calsync_dir,fname + '.ics')
      self._last_message = S_OK
      if not os.path.exists(self._dir):
        os.mkdir(self._dir)
      _idx[self._this] = self
  def changed(self):
      self._next = time.time() + CYCLE_TIME_SEC
      log('queuing',self._name,'until',time.ctime(self._next))
      _queue[self._next] = self._this
  def check_time(self):
      return (time.time() >= self._next)
  def mtime(self,path):
      if os.path.exists(path):
        return os.stat(path).st_mtime
      else:
        return 0
  def last_mtime(self):
      return self.mtime(self._last)
  def this_mtime(self):
      return self.mtime(self._this)
  def hash(self,data):
      if data:
        h = hashlib.sha256()
        h.update(data)
        return h.digest()
      else:
        return None
  def log(self,urlobj,data=None):
      if not data:
        data = urlobj.read()
      logfile = open(os.path.join(self._dir,'log'),'w+')
      logfile.write(urlobj.geturl()+'\n')
      logfile.write(str(urlobj.info())+'\n\n')
      logfile.write(data+'\n')
      logfile.close()
  def message(self,status):
      if status == self._last_message:
        return None
      self._last_message = status
      if status == S_OK:
        return 'OK'
      elif status == S_NOTFOUND:
        return 'not found'
      else:
        return 'status %i' % status


class LocalSyncObj(SyncObj):
  def __init__(self,name,addr):
      SyncObj.__init__(self,name,addr)

  def splitaddr(self,addr):
      '''Parse addr. Get tuple: url,username,password'''
      m = re.match('^(\w+://)(\w+):([^/@]+)@',addr)
      if m:
        return m.group(1)+addr[m.end():], m.group(2), m.group(3)
      else:
        return addr, None, None

  def upload(self,caldata):
      url,username,password = self.splitaddr(self._addr)
      log(url,username,password)
      if username and password:
        password_mgr = urllib2.HTTPPasswordMgrWithDefaultRealm()
        password_mgr.add_password(None, url, username, password)
        handler = urllib2.HTTPBasicAuthHandler(password_mgr)
      else:
        handler = urllib2.HTTPHandler
      opener = urllib2.build_opener(handler)
      request = urllib2.Request(url, data=caldata)
      request.add_header('Content-Type','text/calendar')
      request.get_method = lambda:'PUT'
      urlobj = opener.open(request)
      # Would have thrown on error - log the result.
      self.log(urlobj)

  def sync(self):
      if not self.check_time():
        return S_TOOSOON
      # Check file exists and is more recent than the last version.
      if not os.path.exists(self._this):
        return S_NOTFOUND
      if self.this_mtime() <= self.last_mtime():
        return S_NOCHANGE
      # Read file
      temp = os.path.join(self._dir,'temp')
      shutil.copy2(self._this,temp) # Copy with metadata
      caldata = file(temp).read()
      # Check that file is different from the last version.
      h = self.hash
      if os.path.exists(self._last) and h(caldata)==h(file(self._last).read()):    
        return S_NOCHANGE
      # Upload the data
      self.upload(caldata)
      # Remember it for next time.
      os.rename(temp,self._last)
      return S_OK


class RemoteSyncObj(SyncObj):
  def __init__(self,name,addr):
      SyncObj.__init__(self,name,addr)
  def check_content_type(self,urlinfo):
      if 'Content-Type' not in urlinfo:
        return False
      ct = set( map( string.strip, urlinfo['Content-Type'].split(';') ) )
      return bool( ct.intersection(('text/calendar','text/plain','application/octet-stream')) )
  def sync(self):
      if not self.check_time():
        return S_TOOSOON
      # Download file
      urlobj = urllib.urlopen(self._addr)
      caldata = urlobj.read()
      # Would have thrown on error - log the result.
      self.log(urlobj,caldata)
      if not self.check_content_type(urlobj.info()):
        return S_NOTFOUND
      # Check that file is different from the last version.
      h = self.hash
      if os.path.exists(self._this) and h(caldata)==h(file(self._this).read()):    
        return S_NOCHANGE
      # Save
      temp = os.path.join(self._dir,'temp')
      if os.path.exists(temp):
        os.unlink(temp)
      calfile = open(temp,'w')
      calfile.write(caldata)
      calfile.flush()
      os.fsync(calfile.fileno())
      calfile.close()
      os.chmod(temp,stat.S_IREAD)
      # Move aside the last version of this file, and replace it with the new.
      if os.path.exists(self._this):
        if os.path.exists(self._last):
          os.chmod(self._last, stat.S_IREAD|stat.S_IWRITE)
        shutil.copy2(self._this,self._last) # Copy with metadata
      os.rename(temp,self._this)
      return S_OK


def have_network():
  '''Returns TRUE if the network is available.'''
  import dbus
  old_NM_STATE_CONNECTED = 3
  new_NM_STATE_CONNECTED_GLOBAL = 70
  bus = dbus.SystemBus()
  network_manager = bus.get_object(
    'org.freedesktop.NetworkManager','/org/freedesktop/NetworkManager')
  try:
    # Accept either old or new state enum values.
    state = int(network_manager.state())
    return state in (old_NM_STATE_CONNECTED, new_NM_STATE_CONNECTED_GLOBAL)
  except Exception, ex:
    log('NetworkManager DBUS error. Assuming we have network: ' + repr(ex))
    return True


def read_config():
  conf = open(config_file)
  current_type = LocalSyncObj
  while True:
    line = conf.readline()
    if not line:
      break
    parts = line.split('=',1)
    if not parts:
      continue
    key = parts[0].strip()
    if not key or key[0]=='#':
      continue
    if key.upper()=='LOCAL:':
      current_type = LocalSyncObj
    elif key.upper()=='REMOTE:':
      current_type = RemoteSyncObj
    elif len(parts)==2:
      addr = parts[1].strip()
      _obj.append( current_type(key,addr) )


def do_sync(obj):
  log(obj.__class__.__name__,obj._name)
  try:
    result = obj.sync()
    log(_status[result])
    if result in (S_OK,S_NOTFOUND):
      msg = obj.message(result)
      if msg:
        _messages.append( '%s %s' % (obj._name,msg) )
  except Exception, e:
    log(e)
    _messages.append( '%s got exception %s' % (obj._name,e) )


def do_work(dummy):
  global _messages
  _messages = []
  global _next_full_sync
  global _last_seen
  now = time.time()
  if not have_network()  or  now > _last_seen + (FULL_SYNC_MINS * 60):
    # We don't want to hog the network as soon as it becomes available.
    # Postpone the next full sync.
    _next_full_sync = max(_next_full_sync, now + (NETWORK_GRACE_MINS * 60))
    #log('postponing full sync until '+time.ctime(_next_full_sync))
  elif now < _next_full_sync:
    # See if there is work to do...
    keys = _queue.keys()
    keys.sort()
    done = set()
    for qtime in keys:
      if now < qtime:
        break
      path = _queue[qtime]
      del _queue[qtime]
      if path in done:
        continue
      done.add(path)
      obj = _idx[path]
      do_sync(obj)
  elif _queue:
    # Postpone the full sync until activity ceases.
    _next_full_sync = now + 60
    log('busy, postponing full sync until '+time.ctime(_next_full_sync))
  else:
    # Time for a full sync
    log('full sync for '+time.ctime(_next_full_sync))
    _next_full_sync = now + (FULL_SYNC_MINS * 60)
    for obj in _obj:
      do_sync(obj)
  if _messages:
    n = Notify.Notification.new(
        'Calendar Sync',
        '\n'.join(_messages),
        'dialog-information'
      )
    n.show()
  _last_seen = time.time()


def watch(timeout):
  import pyinotify as pn
  class CalEventProcessor(pn.ProcessEvent):
    def process_IN_CREATE(self, event):
        self.queue(event)
    def process_IN_DELETE(self, event):
        self.queue(event)
    def process_IN_MODIFY(self, event):
        self.queue(event)
    def queue(self,event):
        if event.pathname in _idx:
          _idx[event.pathname].changed()
  wm = pn.WatchManager()
  mask = pn.IN_DELETE | pn.IN_CREATE | pn.IN_MODIFY # watched events
  notifier = pn.Notifier(wm, CalEventProcessor(),timeout=timeout)
  wdd = wm.add_watch(calsync_dir, mask, rec=False)
  # ?? pyinotify's PIDfile handling is rubbish.
  # Need to check whether an active PID is running *this* program, before
  # quitting.
  pid_file_name = os.path.join(working_dir,'PID')
  try:
    # If we have a PID file, and it refers to a running process, and the process
    # is *not* calsync, then start off by deleting the PID file.
    if os.path.exists(pid_file_name):
      pid = file(pid_file_name).read().strip()
      cmdline = file(os.path.join('/proc', pid, 'cmdline')).read().strip()
      if 'calsync' not in cmdline:
        os.unlink(pid_file_name)
  except:
    os.unlink(pid_file_name)

  notifier.loop(
      do_work,
      True, # daemonise
      pid_file=pid_file_name,
      # Uncomment to see output while daemonised:
      #stdout='/dev/stdout', stderr='/dev/stderr'
    )


def init():
  # Set the default timeout so that we don't hang indefinitely waiting for
  # remote web servers.
  import socket
  socket.setdefaulttimeout(300)


if __name__=='__main__':
  if not os.path.exists(working_dir):
    os.mkdir(working_dir)
  if not _log:
    _log = open(os.path.join(working_dir,'log'),'a+')
  init()
  read_config()
  Notify.init("summary-body")
  watch(CYCLE_TIME_SEC * 1000)
