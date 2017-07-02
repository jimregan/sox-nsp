/* libSoX CSL NSP format.
 * http://web.archive.org/web/20160525045942/http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/CSL/CSL.html
 *
 * Copyright 2017 Jim O'Regan
 *
 * based on aiff.c
 * Copyright 1991-2007 Guido van Rossum And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Guido van Rossum And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

#include <time.h>      /* for time stamping comments */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

int lsx_nspstartread(sox_format_t * ft)
{
  char buf[5];
  uint32_t hchunksize;
  uint32_t chunksize;
  unsigned short channels = 0;
  sox_encoding_t enc = SOX_ENCODING_SIGN2;
  unsigned short bits = 16;
  uint32_t frames;
  double rate = 0.0;
  uint32_t offset = 0;
  uint32_t blocksize = 0;
  unsigned short looptype;
  int i, j;
  off_t seekto = 0;
  size_t ssndsize = 0;

  char *date;
  uint16_t maxabschan[8];
  uint32_t datalength;
  int numchannels;

  int rc;
  uint8_t trash8;

  /* FORM chunk */
  if (lsx_reads(ft, buf, (size_t)8) == SOX_EOF || strncmp(buf, "FORMDS16", (size_t)8) != 0) {
    lsx_fail_errno(ft,SOX_EHDR,"NSP header does not begin with magic word `FORMDS16'");
    return(SOX_EOF);
  }
  lsx_readdw(ft, &hchunksize);

  while (1) {
    if (lsx_reads(ft, buf, (size_t)4) == SOX_EOF) {
      if (ssndsize > 0)
        break;
      else {
        lsx_fail_errno(ft,SOX_EHDR,"Missing SDA_ chunk in NSP file");
        return(SOX_EOF);
      }
    }
    if (strncmp(buf, "HEDR", (size_t)4) == 0) {
      /* HEDR chunk */
      lsx_readdw(ft, &chunksize);
      lsx_reads(ft, date, 20);
      lsx_readdw(ft, &(ft->signal.rate));
      lsx_readdw(ft, &datalength);
      lsx_readw(ft, &maxabschan[0]);
      lsx_readw(ft, &maxabschan[1]);

      /* Most likely there will only be 1 channel, but there can be 2 here */
      if (maxabschan[0] == 0xffff && maxabschan[1] == 0xffff) {
        lsx_fail_errno(ft,SOX_EHDR,"Channels A and B undefined");
      } else if (maxabschan[0] == 0xffff || maxabschan[1] == 0xffff) {
        ft->signal.channels = 1;
      } else {
        ft->signal.channels = 2;
      }

    } else if (strncmp(buf, "HDR8", (size_t)4) == 0) {
      /* HDR8 chunk */
      lsx_readdw(ft, &chunksize);
      lsx_reads(ft, date, 20);
      lsx_readdw(ft, &(ft->signal.rate));
      lsx_readdw(ft, &datalength);
      lsx_readw(ft, &maxabschan[0]);
      lsx_readw(ft, &maxabschan[1]);
      lsx_readw(ft, &maxabschan[2]);
      lsx_readw(ft, &maxabschan[3]);
      lsx_readw(ft, &maxabschan[4]);
      lsx_readw(ft, &maxabschan[5]);
      lsx_readw(ft, &maxabschan[6]);
      lsx_readw(ft, &maxabschan[7]);

      /* Can be up to 8 channels */
      numchannels = 0;
      for (i = 0; i < 7; i++) {
        if (maxabschan[i] != 0xffff) {
          numchannels++;
        }
      }
      if (numchannels == 0) {
        lsx_fail_errno(ft,SOX_EHDR,"No channels defined");
      }
      ft->signal.channels = numchannels;

    } else if (strncmp(buf, "NOTE", (size_t)4) == 0) {
      /* NOTE chunk */
      lsx_readdw(ft, &chunksize);
      comment = lsx_malloc(chunksize * sizeof(char*));
      lsx_reads(ft, comment, chunksize);
      lsx_debug("NSP comment: %s", comment);
      free(comment);
    } else if (strncmp(buf, "MARK", (size_t)4) == 0) {
    } else if (strncmp(buf, "INST", (size_t)4) == 0) {
    } else if (strncmp(buf, "COMT", (size_t)4) == 0) {
    } else {
      if (lsx_eof(ft))
        break;
      buf[4] = 0;
      lsx_debug("NSPstartread: ignoring `%s' chunk", buf);
      lsx_readdw(ft, &chunksize);
      if (lsx_eof(ft))
        break;
      /* Skip the chunk using lsx_readb() so we may read
         from a pipe */
      while (chunksize-- > 0) {
        if (lsx_readb(ft, &trash8) == SOX_EOF)
          break;
      }
    }
    if (lsx_eof(ft))
      break;
  }

  /*
   * if a pipe, we lose all chunks after sound.
   * Like, say, instrument loops.
   */
  if (ft->seekable) {
    if (seekto > 0)
      lsx_seeki(ft, seekto, SEEK_SET);
    else {
      lsx_fail_errno(ft,SOX_EOF,"NSP: no sound data on input file");
      return(SOX_EOF);
    }
  }

  ssndsize -= offset;
  while (offset-- > 0) {
    if (lsx_readb(ft, &trash8) == SOX_EOF) {
      lsx_fail_errno(ft,errno,"unexpected EOF while skipping NSP offset");
      return(SOX_EOF);
    }
  }

  return lsx_check_read_params(
      ft, channels, rate, enc, bits, (uint64_t)ssndsize, sox_false);
}
