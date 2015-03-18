import sys

import getopt
import ConfigParser
import re
import time
import datetime
from multiprocessing import Process
import os
import logging
import logging.handlers

from Cam import Cam

camRe = re.compile("cam +(.+)")

def usage():
    print "Usage : %s [-c,--config=<configuration file>] [-h,--help]" % (sys.argv[0])
    sys.exit(1)

if __name__ == "__main__":

    configFile = "fetcher.ini"

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hc:", ["help", "config="])
    except getopt.GetoptError:
        print "Error parsing argument:", sys.exc_info()[1]
        # print help information and exit:
        usage()
        sys.exit(2)

    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit(2)

        if o in ("-c", "--config"):
            configFile = a

    # read and parse config file

    config = ConfigParser.SafeConfigParser()
    config.read(configFile)

    # log stuffs
    formatter = logging.Formatter(fmt='%(asctime)s - %(threadName)s - %(levelname)s - %(message)s')
    # main log
    # logDir = os.environ['HOME'] + "/logs"
    logDir = config.get("globals", "logdir")
    logName = logDir+"/fetcher.log"
    Cam.mkdir(logDir)
    log = logging.getLogger("fetcher")
    log.setLevel(logging.DEBUG)
    fh = logging.handlers.TimedRotatingFileHandler(filename=logName, when=config.get("globals", "rotate"), interval=1, backupCount=5)
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(formatter)
    log.addHandler(fh)

    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    ch.setFormatter(formatter)
    log.addHandler(ch)
    log.info("started!")

    cams = []
    for section in config.sections():
        match = camRe.match(section)
        if match:
            cam_name = match.group(1)
            if config.has_option(section, "enabled") and config.getboolean(section, "enabled"):
                log.info("[%s] - added to control list" % cam_name)
            else:
                log.info("[%s] - not added to fetch list ( not enabled in config file)" % cam_name)
                continue

            url = config.get(section, "url")
            log.info("[%s] - url [%s]" % (cam_name, url))

            hist_size = config.getint(section, "hist_size")
            log.info("[%s] - hist_size  [%d] " % (cam_name, hist_size))

            curl_options = ""
            if config.has_option(section, "curl_options"):
                curl_options = config.get(section, "curl_options")
                log.info("[%s] - curl_options [%s]" % (cam_name, curl_options))

            source_type = config.get(section, "type")
            log.info("[%s] - type [%s]" % (cam_name, source_type))

            raw = config.get(section, "raw")
            log.info("[%s] - raw [%s]" % (cam_name, raw))

            tempdir = config.get(section, "tmp")
            log.info("[%s] - tempdir [%s]" % (cam_name, tempdir))

            chunks_path = config.get(section, "chunks_path")
            log.info("[%s] - chunks_path [%s]" % (cam_name, chunks_path))

            chunk_size = config.getint(section, "chunk_size")
            log.info("[%s] - chunk-size [%s] seconds" % (cam_name, chunk_size))

            m3u8_path = config.get(section, "m3u8_path")
            log.info("[%s] - m3u8_path  [%s]" % (cam_name, m3u8_path))

            webpath_chunks_prefix = config.get(section, "webpath_chunks_prefix")
            log.info("[%s] - webpath_chunks_prefix [%s]" % (cam_name, webpath_chunks_prefix))

            ffmpeg_options = config.get(section, "ffmpeg_options")
            log.info("[%s] - ffmpeg_options  [%s]" % (cam_name, ffmpeg_options ))

            # chunks in m3u8
            m3u8_chunks = config.getint(section, "m3u8_chunks")
            log.info("[%s] - m3u8_chunks [%d] " % (cam_name, m3u8_chunks ))

            fps = config.getint(section, "fps")
            log.info("[%s] - fps [%s]" % (cam_name, fps))

            font = config.get(section, "font")
            log.info("[%s] - font [%s]" % (cam_name, font))

            cam = Cam(cam_name, url, source_type, raw, chunks_path, fps=fps, chunks=m3u8_chunks, chunk_size=chunk_size,
                      m3u8_path=m3u8_path, webpath_chunks_prefix=webpath_chunks_prefix, logger=log, tempdir=tempdir,
                      curl_options=curl_options, font=font, ffmpeg_options=ffmpeg_options, hours=hist_size)
            cams.append(cam)

    # launch a Thread for each cam

    threads = []
    for cam in cams:
        import threading
        t = threading.Thread(target=cam.start, name=cam.name)
        # p = Process(target=cam.start)
        # p.start()
        t.start()
        threads.append(t)

    for t in threads:
        t.join()
