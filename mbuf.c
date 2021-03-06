/******************************************************************************
* Copyright (C) 2008 - 2014 Andreas Öman
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/param.h>

#include "mbuf.h"
#include "trace.h"


/**
 *
 */
void
mbuf_init(mbuf_t *mq)
{
  TAILQ_INIT(&mq->mq_buffers);
  mq->mq_size = 0;
  mq->mq_alloc_size = MBUF_DEFAULT_DATA_SIZE;
}


void
mbuf_set_chunk_size(mbuf_t *m, size_t s)
{
  m->mq_alloc_size = MAX(s, 1024);
}

/**
 *
 */
void
mbuf_data_free(mbuf_t *mq, mbuf_data_t *md)
{
  TAILQ_REMOVE(&mq->mq_buffers, md, md_link);
  free(md->md_data);
  free(md);
}


/**
 *
 */
void
mbuf_clear(mbuf_t *mq)
{
  mbuf_data_t *md;

  mq->mq_size = 0;

  while((md = TAILQ_FIRST(&mq->mq_buffers)) != NULL)
    mbuf_data_free(mq, md);
}


/**
 *
 */
void
mbuf_append(mbuf_t *mq, const void *buf, size_t len)
{
  mbuf_data_t *md = TAILQ_LAST(&mq->mq_buffers, mbuf_data_queue);
  int c;
  mq->mq_size += len;

  if(md != NULL) {
    /* Fill out any previous buffer */
    c = MIN(md->md_data_size - md->md_data_len, len);
    memcpy(md->md_data + md->md_data_len, buf, c);
    md->md_data_len += c;
    buf += c;
    len -= c;
  }
  if(len == 0)
    return;

  md = malloc(sizeof(mbuf_data_t));
  TAILQ_INSERT_TAIL(&mq->mq_buffers, md, md_link);

  c = MAX(len, mq->mq_alloc_size);

  md->md_data = malloc(c);
  md->md_data_size = c;
  md->md_data_len = len;
  md->md_data_off = 0;
  memcpy(md->md_data, buf, len);
}

/**
 *
 */
void
mbuf_append_prealloc(mbuf_t *mq, void *buf, size_t len)
{
  mbuf_data_t *md;

  mq->mq_size += len;

  md = malloc(sizeof(mbuf_data_t));
  TAILQ_INSERT_TAIL(&mq->mq_buffers, md, md_link);

  md->md_data = buf;
  md->md_data_size = len;
  md->md_data_len = len;
  md->md_data_off = 0;
}

/**
 *
 */
size_t
mbuf_read(mbuf_t *mq, void *buf, size_t len)
{
  size_t r = 0;
  int c;

  mbuf_data_t *md;

  while(len > 0) {
    md = TAILQ_FIRST(&mq->mq_buffers);
    if(md == NULL)
      break;

    c = MIN(md->md_data_len - md->md_data_off, len);
    memcpy(buf, md->md_data + md->md_data_off, c);

    r += c;
    buf += c;
    len -= c;
    md->md_data_off += c;
    mq->mq_size -= c;
    if(md->md_data_off == md->md_data_len)
      mbuf_data_free(mq, md);
  }
  return r;
}


/**
 *
 */
int
mbuf_find(mbuf_t *mq, uint8_t v)
{
  mbuf_data_t *md;
  int i, o = 0;

  TAILQ_FOREACH(md, &mq->mq_buffers, md_link) {
    for(i = md->md_data_off; i < md->md_data_len; i++) {
      if(md->md_data[i] == v)
	return o + i - md->md_data_off;
    }
    o += md->md_data_len - md->md_data_off;
  }
  return -1;
}


/**
 *
 */
size_t
mbuf_peek(mbuf_t *mq, void *buf, size_t len)
{
  size_t r = 0;
  int c;

  mbuf_data_t *md = TAILQ_FIRST(&mq->mq_buffers);

  while(len > 0 && md != NULL) {
    c = MIN(md->md_data_len - md->md_data_off, len);
    memcpy(buf, md->md_data + md->md_data_off, c);

    r += c;
    buf += c;
    len -= c;

    md = TAILQ_NEXT(md, md_link);
  }
  return r;
}



/**
 *
 */
size_t
mbuf_peek_tail(mbuf_t *mq, void *buf, size_t len)
{
  size_t r = 0;
  int c;

  len = MIN(mq->mq_size, len);
  if(len == 0)
    return 0;

  mbuf_data_t *md = TAILQ_LAST(&mq->mq_buffers, mbuf_data_queue);
  assert(md != NULL);

  ssize_t remain = len;

  while(1) {
    remain -= md->md_data_len - md->md_data_off;

    if(remain <= 0)
      break;
    md = TAILQ_PREV(md, mbuf_data_queue, md_link);
  }

  size_t offset = -remain;

  while(len > 0) {
    c = MIN(md->md_data_len - (md->md_data_off + offset), len);
    memcpy(buf, md->md_data + (md->md_data_off + offset), c);

    offset = 0;
    r += c;
    buf += c;
    len -= c;

    md = TAILQ_NEXT(md, md_link);
  }

  return r;
}


/**
 *
 */
size_t
mbuf_drop(mbuf_t *mq, size_t len)
{
  size_t r = 0;
  int c;
  mbuf_data_t *md;

  while(len > 0) {
    md = TAILQ_FIRST(&mq->mq_buffers);
    if(md == NULL)
      break;

    c = MIN(md->md_data_len - md->md_data_off, len);
    len -= c;
    md->md_data_off += c;
    mq->mq_size -= c;
    r += c;
    if(md->md_data_off == md->md_data_len)
      mbuf_data_free(mq, md);
  }
  return r;
}


/**
 *
 */
size_t
mbuf_drop_tail(mbuf_t *mq, size_t len)
{
  size_t r = 0;
  int c;
  mbuf_data_t *md;

  while(len > 0) {
    md = TAILQ_LAST(&mq->mq_buffers, mbuf_data_queue);
    if(md == NULL)
      break;

    c = MIN(md->md_data_len - md->md_data_off, len);
    len -= c;
    md->md_data_len -= c;
    mq->mq_size -= c;
    r += c;
    if(md->md_data_off == md->md_data_len)
      mbuf_data_free(mq, md);
  }
  return r;
}

/**
 *
 */
void
mbuf_vqprintf(mbuf_t *mq, const char *fmt, va_list ap)
{
  char buf[10000];
  mbuf_append(mq, buf, vsnprintf(buf, sizeof(buf), fmt, ap));
}


/**
 *
 */
void
mbuf_qprintf(mbuf_t *mq, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  mbuf_vqprintf(mq, fmt, ap);
  va_end(ap);
}


/**
 *
 */
void
mbuf_appendq(mbuf_t *mq, mbuf_t *src)
{
  mbuf_data_t *md;

  mq->mq_size += src->mq_size;
  src->mq_size = 0;

  // XXX: Use TAILQ_MERGE()
  while((md = TAILQ_FIRST(&src->mq_buffers)) != NULL) {
    TAILQ_REMOVE(&src->mq_buffers, md, md_link);
    TAILQ_INSERT_TAIL(&mq->mq_buffers, md, md_link);
  }
}


/**
 *
 */
void
mbuf_dump_raw_stderr(mbuf_t *mq)
{
  mbuf_data_t *md;
  char n = '\n';

  TAILQ_FOREACH(md, &mq->mq_buffers, md_link) {
    if(write(2, md->md_data + md->md_data_off,
	     md->md_data_len - md->md_data_off)
       != md->md_data_len - md->md_data_off)
      break;
  }
  if(write(2, &n, 1) != 1)
    return;
}


/**
 *
 */
void
mbuf_append_and_escape_xml(mbuf_t *mq, const char *s)
{
  const char *c = s;
  const char *e = s + strlen(s);
  if(e == s)
    return;

  while(1) {
    const char *esc;
    switch(*c++) {
    case '<':  esc = "&lt;";   break;
    case '>':  esc = "&gt;";   break;
    case '&':  esc = "&amp;";  break;
    case '\'': esc = "&apos;"; break;
    case '"':  esc = "&quot;"; break;
    default:   esc = NULL;     break;
    }

    if(esc != NULL) {
      mbuf_append(mq, s, c - s - 1);
      mbuf_append(mq, esc, strlen(esc));
      s = c;
    }

    if(c == e) {
      mbuf_append(mq, s, c - s);
      break;
    }
  }
}


/**
 *
 */
void
mbuf_append_and_escape_url(mbuf_t *mq, const char *s)
{
  const char *c = s;
  const char *e = s + strlen(s);
  char C;
  if(e == s)
    return;

  while(1) {
    const char *esc;
    C = *c++;

    if((C >= '0' && C <= '9') ||
       (C >= 'a' && C <= 'z') ||
       (C >= 'A' && C <= 'Z') ||
       C == '_' ||
       C == '~' ||
       C == '.' ||
       C == '-') {
      esc = NULL;
    } else {
      static const char hexchars[16] = "0123456789ABCDEF";
      char buf[4];
      buf[0] = '%';
      buf[1] = hexchars[(C >> 4) & 0xf];
      buf[2] = hexchars[C & 0xf];;
      buf[3] = 0;
      esc = buf;
    }

    if(esc != NULL) {
      mbuf_append(mq, s, c - s - 1);
      mbuf_append(mq, esc, strlen(esc));
      s = c;
    }

    if(c == e) {
      mbuf_append(mq, s, c - s);
      break;
    }
  }
}


/**
 *
 */
void
mbuf_append_and_escape_jsonstr(mbuf_t *mq, const char *str)
{
  const char *s = str;

  mbuf_append(mq, "\"", 1);

  while(*s != 0) {
    if(*s == '"' || *s == '/' || *s == '\\' || *s == '\n' || *s == '\r' || *s == '\t') {
      mbuf_append(mq, str, s - str);

      if(*s == '"')
	mbuf_append(mq, "\\\"", 2);
      else if(*s == '/')
	mbuf_append(mq, "\\/", 2);
      else if(*s == '\n')
	mbuf_append(mq, "\\n", 2);
      else if(*s == '\r')
	mbuf_append(mq, "\\r", 2);
      else if(*s == '\t')
	mbuf_append(mq, "\\t", 2);
      else
	mbuf_append(mq, "\\\\", 2);
      s++;
      str = s;
    } else {
      s++;
    }
  }
  mbuf_append(mq, str, s - str);
  mbuf_append(mq, "\"", 1);
}


/**
 *
 */
const void *
mbuf_pullup(mbuf_t *mq, size_t bytes)
{
  if(mq->mq_size < bytes || bytes == 0)
    return NULL;

  mbuf_data_t *md = TAILQ_FIRST(&mq->mq_buffers);
  size_t avail = md->md_data_len - md->md_data_off;
  if(avail >= bytes) {
    // Front buffer have enough contig bytes
    return md->md_data + md->md_data_off;
  }

  void *data = malloc(bytes);
  mbuf_read(mq, data, bytes);
  md = malloc(sizeof(mbuf_data_t));
  TAILQ_INSERT_HEAD(&mq->mq_buffers, md, md_link);
  md->md_data = data;
  md->md_data_size = bytes;
  md->md_data_len = bytes;
  md->md_data_off = 0;
  mq->mq_size += bytes;
  return data;
}


/**
 * This can be optimized similar to mbuf_pullup
 */
char *
mbuf_clear_to_string(mbuf_t *mq)
{
  mbuf_append(mq, "", 1);
  char *r = malloc(mq->mq_size);
  mbuf_read(mq, r, mq->mq_size);
  mbuf_clear(mq);
  return r;
}


void
mbuf_hexdump(const char *prefix, mbuf_t *mq)
{
  mbuf_data_t *md;
  TAILQ_FOREACH(md, &mq->mq_buffers, md_link) {
    hexdump(prefix, md->md_data + md->md_data_off,
            md->md_data_len - md->md_data_off);
  }
}
