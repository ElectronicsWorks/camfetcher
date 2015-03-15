/** versions
**           Created mjpeg2jpg.c <-- readability.c           2011-02-03
**           Extended to conversion of Intellinet mp4 files. 2012-12-14
**           Argument "--help" now gives Usage().            2014-06-24
**/

/*******************************************************************************
** Copyright (C) 2011 by Harm J. Schoonhoven
**  
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
** 
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
** 
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** Mail to: info@harmjschoonhoven.com
**
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#define FORI(n) for(i=0;i<(n);i++)

#define MAXS					256
#define BUFFERBLOCKSIZE			50000
#define MAXLINELENGTH			50
#define DFLTEND					8640000

#define PAYLOAD_AUDIO			0
#define PAYLOAD_JPEG			1
#define PAYLOAD_MP4				2
#define PAYLOAD_UNDEFINED 		3
#define NPAYLOAD				3

char *typestr[NPAYLOAD + 1] = {
  "audio/basic",
  "image/jpeg",
  "video/mpeg",
  "nothing"
};

FILE *src = NULL;
char *srcfname;
char *destfname;
char *buffer = NULL;
int allocated;
int nbuffer;
int picturenumber = 0;
int minsize = 0, maxsize = 0, sumsize = 0;
int begin = 0;
int end = DFLTEND;
int filenumber = 0;
int increment = 1;
int jump = 1;
double percentage = 0.0;
BOOL silent = FALSE;
BOOL statist = FALSE;
BOOL single;
int addtime = 0;
int payload = PAYLOAD_UNDEFINED;
BOOL largefile = FALSE;

int Setmode (char *arg);
int Setintvalue (int *pv, char *arg);
BOOL Readcontent (void);
char *Printable (char *s);
void Writedata (void);
void Mjpeg (void);
void Mp4 (void);
void Destroy (void);
void ShowKV (void);
int Usage (void);

int
main (int argc, char *argv[]) {
  int nok, i;
  long where;

  if (argc == 2 && !strcmp (argv[1], "--help"))
    return (Usage ());
  if (argc == 2 || (argc == 4 && !strncmp (argv[2], "-l", 2))) {
    if (argc == 4) {
      nok = Setmode (argv[3]);
      if (nok)
	return (nok);
    }
    destfname = NULL;
  }
  else if (argc >= 3) {
    destfname = argv[2];
    single = 0;
    for (i = 3; i + 1 < argc; i += 2) {
      nok = 0;
      if (*argv[i] != '-') {
	fprintf (stderr, "-<KEY> expected in \"%s\".\n", argv[i]);
	nok = 2;
      }
      else {
	switch (*(argv[i] + 1)) {
	case 'b':
	  nok = Setintvalue (&begin, argv[i + 1]);
	  break;
	case 'e':
	  nok = Setintvalue (&end, argv[i + 1]);
	  break;
	case 'f':
	  nok = Setintvalue (&filenumber, argv[i + 1]);
	  break;
	case 'i':
	  if (*argv[i + 1] == 't') {
	    addtime = 1;
	    increment = 0;
	  }
	  else
	    nok = Setintvalue (&increment, argv[i + 1]);
	  break;
	case 'j':
	  nok = Setintvalue (&jump, argv[i + 1]);
	  break;
	case 'l':
	  nok = Setmode (argv[i + 1]);
	  break;
	case 's':
	  if (1 != sscanf (argv[i + 1], "%lf", &percentage)) {
	    fprintf (stderr,
		     "Cannot read <percentage> in \"%s\".\n", argv[i + 1]);
	    return (2);
	  }
	  break;
	case 't':
	  switch (*argv[i + 1]) {
	  case 'a':
	    payload = PAYLOAD_AUDIO;
	    break;
	  case 'j':
	    payload = PAYLOAD_JPEG;
	    break;
	  case 'm':
	    payload = PAYLOAD_MP4;
	    break;
	  default:
	    fprintf (stderr, "Illegal <VALUE> \"%s\".\n", argv[i + 1]);
	    nok = 2;
	    break;
	  }
	  break;
	default:
	  fprintf (stderr, "Illegal <KEY> \"%s\".\n", argv[i]);
	  nok = 2;
	  break;
	}
      }
      if (nok)
	return (nok);
    }
  }
  else
    return (Usage ());
  srcfname = argv[1];
  if (strcmp (srcfname, "-")) {
    src = fopen (srcfname, "rb");
    if (!src) {
      fprintf (stderr, "Cannot open \"%s\" to read.\n", srcfname);
      return (3);
    }
  }
  else {
    srcfname = "<stdin>";
    src = stdin;
  }
  allocated = BUFFERBLOCKSIZE;
  buffer = (char *) realloc (buffer, allocated * sizeof (char));
  if (!buffer) {
    fprintf (stderr, "Allocating %d bytes failed.\n", allocated);
    Destroy ();
    return (4);
  }
  for (;;) {
    if (picturenumber >= end) {
      break;
    }
    if (feof (src)) {
      break;
    }
    if (Readcontent ()) {
      if (nbuffer) {
	if (!silent) {
	  if (!largefile) {
	    where = ftell (src);
	    if (where >= 2000000000)
	      largefile = TRUE;
	    if (!largefile) {
	      // printf("picture/videoblock %9d with %9d bytes. Src at %ld\n",picturenumber,nbuffer,ftell(src));
	    }
	  }
	  if (largefile) {
	    printf ("picture/videoblock %9d with %9d bytes.\n",
		    picturenumber, nbuffer);
	  }
	}
	if (picturenumber) {
	  if (nbuffer < minsize)
	    minsize = nbuffer;
	  if (nbuffer > maxsize)
	    maxsize = nbuffer;
	  sumsize += nbuffer;
	}
	else {
	  minsize = nbuffer;
	  maxsize = nbuffer;
	  sumsize = nbuffer;
	}
	if (destfname && picturenumber == begin) {
	  if (percentage <= 0.0
	      || maxsize >= (1.0 + 0.01 * percentage) * minsize)
	    Writedata ();
	  if (single && payload == PAYLOAD_JPEG) {
	    printf ("Single JPEG-file: \"%s\".\n", destfname);
	    break;
	  }
	  if (addtime) {
	    printf ("Time added: \"%s\".\n", destfname);
	    break;
	  }
	  filenumber += increment;
	  begin += jump;
	}
	picturenumber++;
      }
      else {
	printf ("Empty buffer.\n");
	break;
      }
    }
  }
  if (feof (src)) {
    printf ("End of src \"%s\" reached.\n", srcfname);
  }
  else {
    printf ("%9d picture(s) [incomplete]\n", picturenumber);
  }
  if (statist && picturenumber) {
    printf ("%s extracted from %s \n", typestr[payload], srcfname);
    printf ("minimum payload size %d bytes\n", minsize);
    printf ("maximum payload size %d bytes\n", maxsize);
    printf ("average payload size %d bytes\n", sumsize / picturenumber);
  }

  Destroy ();
  return (0);
}

int
Setmode (char *arg) {
  switch (*arg) {
  case 'q':
    silent = TRUE;
    break;
  case 's':
    statist = TRUE;
    break;
  default:
    fprintf (stderr, "Illegal <mode> in \"%s\".\n", arg);
    return (2);
    break;
  }
  return (0);
}

int
Setintvalue (int *pv, char *arg) {
  if (1 != sscanf (arg, "%d", pv)) {
    fprintf (stderr, "Cannot read integer in \"%s\".\n", arg);
    return (2);
  }
  return (0);
}

/* DOCU Readcontent()
	returns TRUE if intended Content-Type is found.
	returns with nbuffer set to 0 in case of error.
*/
BOOL
Readcontent (void) {
  int i, contenttype, nbyte, clen, el = 0;
  char s[MAXLINELENGTH];
  nbuffer = 0;
  s[MAXLINELENGTH - 1] = 0;
  contenttype = PAYLOAD_JPEG;
  nbyte = 0;

  while (1) {
    fgets (s, MAXLINELENGTH, src);

    //fprintf (stderr, "red line[ %s] %ld\n", Printable (s), strlen (s));
    if (strlen (s) <= 2 && nbyte) {
      break;
    }
    else if (sscanf (s, "%*[A-Z_a-z/:- ]%d", &clen) == 1) {
      //fprintf (stderr, "red Content-Length %d", clen);
      nbyte = clen;
    }
  }
  if (!nbyte) {
    fprintf (stderr, "Cannot read Content-Length");
    return (TRUE);
  }

  if (nbyte > allocated) {
    allocated =
      BUFFERBLOCKSIZE * ((nbyte + BUFFERBLOCKSIZE) / BUFFERBLOCKSIZE);
    buffer = (char *) realloc (buffer, allocated * sizeof (char));
    if (!buffer) {
      fprintf (stderr, "Allocating %d bytes failed.\n", allocated);
      return (TRUE);
    }
  }
  nbuffer = fread (buffer, sizeof (char), nbyte, src);
  if (nbuffer != nbyte) {
    fprintf (stderr, "Cannot read %d bytes.\n", nbyte);
    nbuffer = 0;
    return (TRUE);
  }
  fgets (s, MAXLINELENGTH, src);
  return (TRUE);
}

char *
Printable (char *s) {
  char *c;
  for (c = s; *c; c++)
    if (*c < ' ' || *c > '~')
      *c = '?';
  return (s);
}

static void
_mkdir (const char *dir) {
  char tmp[256];
  char *p = NULL;
  size_t len;

  snprintf (tmp, sizeof (tmp), "%s", dir);
  len = strlen (tmp);
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;
  for (p = tmp + 1; *p; p++)
    if (*p == '/') {
      *p = 0;
      mkdir (tmp, S_IRWXU);
      *p = '/';
    }
  mkdir (tmp, S_IRWXU);
}

char *
getfname (char *base) {

  struct timeval tv;
  char c_time_string[200];
  static char out[200];
  struct tm *gmt;

  gettimeofday (&tv, NULL);
  gmt = gmtime (&tv.tv_sec);

  strftime (c_time_string, 200, "%Y/%m/%d/%H/%M/%S", gmt);
  sprintf (out, "%s/%s", base, c_time_string);
  _mkdir (out);

  //strftime(c_time_string,200,"%Y-%m-%d_%H:%M:%S",gmt);
  sprintf (out, "%s/%s/%.3ld.jpg", base, c_time_string, tv.tv_usec);

  return out;

}

void
Writedata (void) {
  struct timeval tv;
  int countperc, seconds, hh, mm, ss;
  char *c;
  FILE *dest;
  char *fn;
  char *fname;

  fn = getfname (destfname);
  //sprintf (fn,"%s/%s",destfname, fname);
  // printf ("\t%s\n",fn);
  dest = fopen (fn, "ab");
  if (dest) {
    if (nbuffer != fwrite (buffer, sizeof (char), nbuffer, dest)) {
      fprintf (stderr, "Failed to write %d bytes to \"%s\".\n", nbuffer, fn);
    }
    fclose (dest);
  }
  else {
    fprintf (stderr, "Cannot open \"%s\" to write.\n", fn);
  }
}

void
Destroy (void) {
  if (src && src != stdin)
    fclose (src);
  if (buffer) {
    free (buffer);
    buffer = NULL;
  }
}

void
ShowKV (void) {
  printf ("KEY          VALUE           DESCRIPTION AND DEFAULT\n");
  printf ("-b[egin]     <first picture> picturenumber to save. Default: 0\n");
  printf ("-e[nd]       <last picture>  terminate process. Default: %d\n",
	  DFLTEND);
  printf ("-f[irst]     <filenumber>    in filename <dest>. Default: 0\n");
  printf
    ("-i[ncrement] <count>         increment of <filenumber>. Default: 1\n");
  printf
    ("-i[ncrement] t[ime]          add seconds in day to first <filenumber>.\n");
  printf
    ("-j[ump]      <pictures>      increment picturenumber to save. Default: 1\n");
  printf ("-l[ist]      q[uiet]         do not write progress to stdout.\n");
  printf ("-l[ist]      s[tatist]       write statistics to stdout.\n");
  printf
    ("-s[elect]    <percentage>    write <dest> only if maximumJPGpayloadsize >=\n");
  printf
    ("                             (1+0.01*<percentage>)*minimumJPGpayloadsize\n");
  printf
    ("                             calculated over <mjpg-file> as far as read.\n");
  printf ("-t[ype]      a[udio]         extract Content-Type %s.\n",
	  typestr[PAYLOAD_AUDIO]);
  printf ("-t[ype]      j[peg]          extract Content-Type %s.\n",
	  typestr[PAYLOAD_JPEG]);
  printf ("-t[ype]      m[peg]          extract Content-Type %s.\n",
	  typestr[PAYLOAD_MP4]);
  printf ("                             By default %s or %s is extracted\n",
	  typestr[PAYLOAD_JPEG], typestr[PAYLOAD_MP4]);
  printf ("                             whichever is found first.\n");
}

int
Usage (void) {
  char **pc;
  char *usage[] = {
    "USAGE: mjpeg2jpg <mjpg-file> [-l[ist] <mode>]",
    "       verify and/or list properties of <mjpg-file>.",
    "USAGE: mjpeg2jpg <mjpg-file> <dest> [<key> <value> ..]",
    "       extract one or more JPG-files from <mjpg-file>.",
    "DEST:",
    "  \"[<path><slash>][<prefix>]%[[0]<width>]d[<suffix>].jpg\"",
    "  %d replaced by <filenumber> or a single file",
    "  \"<basename>.jpg\"",
    "USAGE: mjpeg2jpg <Intellinet mp4-file> <dest mp4-file> [<key> <value> ..]",
    "       extract mp4-stream from downloaded file.",
    (char *) ShowKV,
    "",
    "    Error messages are written to stderr.",
    "",
    "SYNTAX OF MJPEG-FILES:",
    "   \"--ThisRandomString\" or \"--myboundary\"",
    "   \"Content-Type: image/jpeg\"",
    "   \"Content-Length: ######\"",
    "   \"\"",
    "   ###### bytes JPG-file payload",
    "   \"\"    All lines end on \\r\\n (0x0d0a).",
    "",
    "SYNTAX OF \"MP4\"-FILES:",
    "   \"--myboundary\"",
    "   \"Content-Type: image/mpeg\" or",
    "   \"Content-Type: audio/basic\" (ignored by default)",
    "   \"Content-Length: #[#####]\"",
    "   \"\"",
    "   #[#####] bytes JPG-file payload",
    "   \"\"    All lines end on \\r\\n (0x0d0a).",
    "",
    "NOTE: Use ffmpeg 0.5 to extract pictures from an Intellinet IP-Cam MP4-stream.",
    "Copyright (C) 2011 by Harm J. Schoonhoven",
    "",
    "Permission is hereby granted, free of charge, to any person obtaining a copy",
    "of this software and associated documentation files (the \"Software\"), to deal",
    "in the Software without restriction, including without limitation the rights",
    "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell",
    "copies of the Software, and to permit persons to whom the Software is",
    "furnished to do so, subject to the following conditions:",
    "",
    "The above copyright notice and this permission notice shall be included in",
    "all copies or substantial portions of the Software.",
    "",
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR",
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,",
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE",
    "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER",
    "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,",
    "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN",
    "THE SOFTWARE.",
    NULL
  };
  for (pc = usage; *pc; pc++) {
    if (*pc == (char *) ShowKV)
      ShowKV ();
    else
      printf ("%s\n", *pc);
  }
  printf ("VERSION: %s\n", __DATE__);
  return (1);
}

/* copyright H.J.Schoonhoven, Utrecht, 2011. */


