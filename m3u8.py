
import os
# https://developer.apple.com/library/ios/technotes/tn2288/_index.html

class m3u8:
    def __init__(self, targetduration=10.0, media_sequence=0, nseq=3, fname="m3u8.m3u8"):
        self.targetduration = float(targetduration)
        self.media_sequence = media_sequence
        self.nseq = nseq
        self.fname = fname
        self.media_array = []

    def addstream(self, url):
        self.media_array.append(url)
        if len(self.media_array) > self.nseq:
            self.media_array.pop(0)
            self.media_sequence += 1

    def write(self):
        tmpfname = self.fname+".tmp"
        f = open(tmpfname, "w")
        f.write(self.str())
        f.close()
        os.rename(tmpfname, self.fname)


    def str(self):
        out = []
        out.append("#EXTM3U")
        out.append("#EXT-X-VERSION:3")
        out.append("#EXT-X-TARGETDURATION:%.3f" % self.targetduration)
        out.append("#EXT-X-MEDIA-SEQUENCE:%d" % self.media_sequence)
        for media in self.media_array:
            out.append("#EXTINF:%.3f," % self.targetduration)
            out.append(media)

        return '\n'.join(out)
