import os
import logging
import time
import subprocess
import threading
import re
import datetime
import pytz
import shutil
from m3u8 import m3u8


class Cam:
    @staticmethod
    def mkdir(d):
        if os.path.exists(d):
            if not os.path.isdir(d):
                raise NameError("%s must be a directory" % d)
        else:
            os.makedirs(d)

    @staticmethod
    def simplelog(*args):
        print args[0]

    def log(self):
        if self.log:
            return self.log
        else:
            return logging.getLogger(self.name)

    def __init__(self, name, url, source_type, raw, chunks_path, fps=5, chunk_size=10, chunks=3, hours=24, logger=None,
                 tempdir=None, m3u8_path=None, webpath_chunks_prefix="", font=None, curl_options="", ffmpeg_options=None):

        if ffmpeg_options is None:
            ffmpeg_options = "-c:v libx264 -crf 18 -profile:v baseline -maxrate 128k -bufsize 256k -pix_fmt yuv420p"
        self.ffmpeg_options = ffmpeg_options
        self.name = name
        self.url = url
        self.source_type = source_type
        self.raw = os.path.abspath(raw)
        self.chunks_path = os.path.abspath(chunks_path)
        self.chunk_size = chunk_size
        self.hours = hours
        self.fps = fps
        self.font = font
        self.curl_options = curl_options
        if tempdir is None:
            tempdir = raw+"/tmp"
        self.tempdir = os.path.abspath(tempdir)
        if m3u8_path is None:
            m3u8_path = name
        self.m3u8_path = m3u8_path
        self.webpath_chunks_prefix = webpath_chunks_prefix

        self.m3u8 = m3u8(targetduration=chunk_size, nseq=chunks, fname=self.m3u8_path)
        # create paths if not exists
        Cam.mkdir(self.raw)
        Cam.mkdir(self.tempdir)
        Cam.mkdir(self.chunks_path)
        self.log = logger
        self.pid = None

    def do_chunks(self):
        # wait for chunk_size
        while True:
            t = int(time.time())
            if t % self.chunk_size:
                time.sleep(.1)
                continue
            self.log.debug("[%s]: chunk [%d] fetcher pid [%s]" % (self.name, t, self.pid))
            ti = t - self.chunk_size
            period = 1 / float(self.fps)
            # make IMage Array index -> (imagetime, imagename)
            # save directories for removal
            imad = []
            ima = []
            while ti < t:
                # convert ti in gmtime and directory
                gmt = time.gmtime(ti)
                ddir = "%s/%04d/%02d/%02d/%02d/%02d/%02d" % (self.raw, gmt.tm_year, gmt.tm_mon, gmt.tm_mday, gmt.tm_hour, gmt.tm_min, gmt.tm_sec)
                imad.append(ddir)
                # self.log.debug("[%s]: directory for sec %d: [%s]" % (self.name, ti, ddir))
                for root, dirs, files in os.walk(ddir):
                    for name in files:
                        if name.endswith(".jpg"):
                            usec = int(name[:-4])
                            # self.log.debug("[%s]: found FILE [%s], usec [%d]" % (self.name, name, usec))
                            f = "%s/%s" % (root, name)
                            ima.append((f, float(ti+float(usec)/1000000)))
                ti += 1
            # self.log.debug("[%s]: images for chunk [%s]" % (self.name, ima))
            # links to images to make chunks
            if not len(ima):
                if self.pid:
                    self.log.warning("[%s]  killing pid [%d]" % (self.name, self.pid))
                    try:
                        import signal
                        os.kill(self.pid, signal.SIGTERM) # SIGKILL ?
                    except:
                        self.log.exception("[%s]  while killing pid [%d]" % (self.name, self.pid))

                    self.pid = None
                    time.sleep(1)
                    continue

            t = float(t)
            ti = float(t - self.chunk_size)
            outim = []
            while ti < t:
                # search image nearest to "ti"
                td = 999.0
                imname = None
                for name, tim in ima:
                    # self.log.debug("[%s]: searching for image nearesto to [%f]" % (self.name, ti))
                    dt = abs(tim-ti)
                    if dt < td:
                        td = dt
                        imname = name
                if imname:
                    # self.log.debug("[%s]: found  [%s]" % (self.name, imname))
                    outim.append(imname)
                ti += period

            # self.log.debug("[%s]: ordered images for chunk [%s]" % (self.name, outim))
            # now gen symlink in tmp dir
            i = 0
            for imax in outim:
                tmpfname = "%s/%d.jpg" % (self.tempdir, i)
                if os.path.lexists(tmpfname):
                    os.remove(tmpfname)
                os.symlink(imax, tmpfname)
                i += 1
            # generate chunk with ffmpeg
            gmt = time.gmtime(t)
            chunkdir = "%04d-%02d-%02d_%02d" % (gmt.tm_year, gmt.tm_mon, gmt.tm_mday, gmt.tm_hour)
            chunkname = "%04d-%02d-%02d_%02d-%02d-%02d.ts" % (gmt.tm_year, gmt.tm_mon, gmt.tm_mday, gmt.tm_hour, gmt.tm_min, gmt.tm_sec)
            chunkdirpath = "%s/%s" % (self.chunks_path, chunkdir)
            chunkfullpath = "%s/%s" % (chunkdirpath, chunkname)
            chunkwebpath = "%s/%s/%s" % (self.webpath_chunks_prefix, chunkdir, chunkname)
            # make chunk dir if not exists
            Cam.mkdir(chunkdirpath)
            frames = len(outim)
            wtime = time.localtime(t)
            ptso = wtime.tm_hour*3600 + wtime.tm_min*60 + wtime.tm_sec
            cmd = "ffmpeg -loglevel panic -y -v verbose -framerate %d  -start_number 0 -i \"%s/%%d.jpg\" -frames %d " \
                  "-vf setpts=PTS+%d/TB," \
                  "drawbox=t=20:x=0:y=0:width=200:height=30:color=black@0.5," \
                  "drawtext=\"fontfile=%s:expansion=normal:text=%%{pts\\\\\\:hms}:y=10:x=10:fontcolor=yellow\" " \
                  "%s " \
                  "%s" % (self.fps, self.tempdir, frames, ptso, self.font, self.ffmpeg_options, chunkfullpath)
            self.log.debug("[%s]: ffmpeg cmd: [%s]" % (self.name, cmd))
            os.system(cmd)
            # update m3u8
            self.m3u8.addstream(chunkwebpath)
            self.m3u8.write()
            # remove work images
            for imax, imt in ima:
                if os.path.exists(imax):
                    # self.log.debug("[%s]: removing : [%s]" % (self.name, imax))
                    os.remove(imax)
            # remove dirs
            for dirname in imad:
                if os.path.exists(dirname):
                    # self.log.debug("[%s]: removing dir: [%s]" % (self.name, dirname))
                    os.rmdir(dirname)

            while True:
                t = int(time.time())
                if t % self.chunk_size:
                    break
                time.sleep(.1)

    def mjpeg_fetcher(self):
        cmd = "curl %s -s \"%s\" | ./mjpeg2jpg/mjpeg2jpg - %s" % (self.curl_options, self.url, self.raw)
        self.log.debug("[%s]: mjpeg_fetcher [%s] started" % (self.name, cmd))

        while True:
            proc = subprocess.Popen(cmd, shell=True)
            self.log.info("[%s] - spawned pid [%d]" % (self.name, proc.pid))
            self.pid = proc.pid
            proc.wait()
            # os.system(cmd)
            time.sleep(5)

    def do_mjpeg(self):
        self.log.debug("[%s]: do_mjpeg" % self.name)
        # run background mjpeg2 executable
        ## p = Process(target=self.mjpeg_fetcher)
        ## p.start()
        threading.Thread(target=self.mjpeg_fetcher).start()
        # run garbage collector
        threading.Thread(target=self.garbage_collector).start()
        # chunks
        try:
            self.do_chunks()
        except:
            self.log.exception("[%s]:" % self.name)

    # remove chunks
    def garbage_collector(self):
        dir_re = re.compile("([\d]{4})-([\d]{2})-([\d]{2})_([\d]{2})")
        utcnow = datetime.datetime.now(tz=pytz.timezone("GMT"))
        tdref = datetime.timedelta(hours=self.hours)
        self.log.debug("[%s]: os.walk dir utcnow [%s], tdref [%s]" % (self.name, utcnow, tdref))
        while True:
            # scan chunk dir
            for dirpath, dirnames, files in os.walk(self.chunks_path):
                # self.log.debug("[%s]: os.walk [%s] [%s] [%s]" % (self.name, dirpath, dirnames, files))
                for dirname in dirnames:
                    # dirname format is: AAAA-MM-DD_HH e.g. 2015-03-11_14
                    match = dir_re.match(dirname)
                    if match:
                        fullpath = os.path.join(dirpath, dirname)
                        self.log.debug("[%s]: os.walk match dir [%s] [%s]" % (self.name, dirname, fullpath))
                        yyyy = int(match.group(1))
                        mm = int(match.group(2))
                        dd = int(match.group(3))
                        hh = int(match.group(4))
                        ddt = datetime.datetime(yyyy, mm, dd, hh, tzinfo=pytz.timezone("GMT"))
                        td = utcnow - ddt
                        self.log.debug("[%s]: os.walk dir datetime [%s] delta [%s]" % (self.name, ddt, td))
                        if td > tdref:
                            self.log.debug("[%s]: os.walk dir to delete [%s]" % (self.name, fullpath))
                            try:
                                shutil.rmtree(fullpath)
                            except:
                                self.log.exception("[%s] - exception removing tree" % self.name)

            time.sleep(self.hours*360)


    def start(self):
        self.log.debug("[%s]: started" % self.name)
        # type check
        fname = "do_"+self.source_type
        if hasattr(self, fname):
            getattr(self, fname)()
        else:
            self.log.error("[%s]: type not managed" % self.name, self.source_type)



