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

int lsx_nspstartread(sox_format_t * ft);

int lsx_nspstartread(sox_format_t * ft)
{
  char buf[5];
  uint32_t hchunksize;
  uint32_t chunksize;
  unsigned short channels = 0;
  sox_encoding_t enc = SOX_ENCODING_SIGN2;
  unsigned short bits = 16;
  double rate = 0.0;
  uint64_t seekto = 0;
  int i;
  size_t ssndsize = 0;

  char date[20];
  char *comment;
  uint16_t maxabschan[8];
  uint32_t datalength;
  uint32_t samplerate;
  int numchannels;

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
      lsx_reads(ft, date, (size_t)20);
      lsx_readdw(ft, &samplerate);
      rate = (double)samplerate;
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
      lsx_reads(ft, date, (size_t)20);
      lsx_readdw(ft, &samplerate);
      rate = (double)samplerate;
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
      unsigned char nullc = 0;
      /* NOTE chunk */
      lsx_readdw(ft, &chunksize);
      lsx_debug("NSP chunksize: %d", chunksize);
      comment = lsx_malloc(chunksize * sizeof(char*));
      lsx_reads(ft, comment, (size_t)chunksize);
      if(strlen(comment) != 0)
        lsx_debug("NSP comment: %s %d", comment);
      free(comment);
      lsx_readb(ft, &nullc);
    } else if (strncmp(buf, "SDA_", (size_t)4) == 0) {
      lsx_readdw(ft, &chunksize);
      ssndsize = chunksize;
      /* if can't seek, just do sound now */
      if (!ft->seekable)
        break;
      /* else, seek to end of sound and hunt for more */
      seekto = lsx_tell(ft);
      lsx_seeki(ft, (off_t)chunksize, SEEK_CUR);
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

  if (ft->seekable) {
    if (seekto > 0)
      lsx_seeki(ft, seekto, SEEK_SET);
    else {
      lsx_fail_errno(ft,SOX_EOF,"NSP: no sound data on input file");
      return(SOX_EOF);
    }
  }

  return lsx_check_read_params(
      ft, channels, rate, enc, bits, (uint64_t)ssndsize/2, sox_false);
}

static int lsx_nspstopread(sox_format_t * ft)
{
    ft->sox_errno = SOX_SUCCESS;

    return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(nsp)
{
  static char const * const names[] = {"nsp", NULL };
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Computerized Speech Lab NSP file",
    names, SOX_FILE_LIT_END,
    lsx_nspstartread, lsx_rawread, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL, 0
  };
  return &handler;
}
