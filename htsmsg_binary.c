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
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "htsmsg_binary.h"

/*
 *
 */
static int
htsmsg_binary_des0(htsmsg_t *msg, const uint8_t *buf, size_t len)
{
  unsigned type, namelen, datalen;
  htsmsg_field_t *f;
  htsmsg_t *sub;
  char *n;
  uint64_t u64;
  int i;

  while(len > 5) {

    type    =  buf[0];
    namelen =  buf[1];
    datalen = (buf[2] << 24) |
              (buf[3] << 16) |
              (buf[4] << 8 ) |
              (buf[5]      );
    
    buf += 6;
    len -= 6;
    
    if(len < namelen + datalen)
      return -1;

    f = malloc(sizeof(htsmsg_field_t));
    f->hmf_type  = type;

    if(namelen > 0) {
      n = malloc(namelen + 1);
      memcpy(n, buf, namelen);
      n[namelen] = 0;

      buf += namelen;
      len -= namelen;
      f->hmf_flags = HMF_NAME_ALLOCED;

    } else {
      n = NULL;
      f->hmf_flags = 0;
    }

    f->hmf_name  = n;

    switch(type) {
    case HMF_STR:
      f->hmf_str = n = malloc(datalen + 1);
      memcpy(n, buf, datalen);
      n[datalen] = 0;
      f->hmf_flags |= HMF_ALLOCED;
      break;

    case HMF_BIN:
      f->hmf_bin = (const void *)buf;
      f->hmf_binsize = datalen;
      break;

    case HMF_S64:
      u64 = 0;
      for(i = datalen - 1; i >= 0; i--)
	  u64 = (u64 << 8) | buf[i];
      f->hmf_s64 = u64;
      break;

    case HMF_MAP:
    case HMF_LIST:
      sub = &f->hmf_msg;
      TAILQ_INIT(&sub->hm_fields);
      sub->hm_free_opaque = NULL;
      if(htsmsg_binary_des0(sub, buf, datalen) < 0)
	return -1;
      break;

    default:
      free(n);
      free(f);
      return -1;
    }

    TAILQ_INSERT_TAIL(&msg->hm_fields, f, hmf_link);
    buf += datalen;
    len -= datalen;
  }
  return 0;
}



/*
 *
 */
htsmsg_t *
htsmsg_binary_deserialize(const void *data, size_t len, void *buf)
{
  htsmsg_t *msg = htsmsg_create_map();
  msg->hm_opaque = buf;
  msg->hm_free_opaque = &free;

  if(htsmsg_binary_des0(msg, data, len) < 0) {
    htsmsg_destroy(msg);
    return NULL;
  }
  return msg;
}



/*
 *
 */
static size_t
htsmsg_binary_count(htsmsg_t *msg)
{
  htsmsg_field_t *f;
  size_t len = 0;
  uint64_t u64;

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {

    len += 6;
    len += f->hmf_name ? strlen(f->hmf_name) : 0;

    switch(f->hmf_type) {
    case HMF_MAP:
    case HMF_LIST:
      len += htsmsg_binary_count(&f->hmf_msg);
      break;

    case HMF_STR:
      len += strlen(f->hmf_str);
      break;

    case HMF_BIN:
      len += f->hmf_binsize;
      break;

    case HMF_S64:
      u64 = f->hmf_s64;
      while(u64 != 0) {
	len++;
	u64 = u64 >> 8;
      }
      break;
    }
  }
  return len;
}


/*
 *
 */
static void
htsmsg_binary_write(htsmsg_t *msg, uint8_t *ptr)
{
  htsmsg_field_t *f;
  uint64_t u64;
  int l, i, namelen;

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {
    namelen = f->hmf_name ? strlen(f->hmf_name) : 0;
    *ptr++ = f->hmf_type;
    *ptr++ = namelen;

    switch(f->hmf_type) {
    case HMF_MAP:
    case HMF_LIST:
      l = htsmsg_binary_count(&f->hmf_msg);
      break;

    case HMF_STR:
      l = strlen(f->hmf_str);
      break;

    case HMF_BIN:
      l = f->hmf_binsize;
      break;

    case HMF_S64:
      u64 = f->hmf_s64;
      l = 0;
      while(u64 != 0) {
	l++;
	u64 = u64 >> 8;
      }
      break;
    default:
      abort();
    }


    *ptr++ = l >> 24;
    *ptr++ = l >> 16;
    *ptr++ = l >> 8;
    *ptr++ = l;

    if(namelen > 0) {
      memcpy(ptr, f->hmf_name, namelen);
      ptr += namelen;
    }

    switch(f->hmf_type) {
    case HMF_MAP:
    case HMF_LIST:
      htsmsg_binary_write(&f->hmf_msg, ptr);
      break;

    case HMF_STR:
      memcpy(ptr, f->hmf_str, l);
      break;

    case HMF_BIN:
      memcpy(ptr, f->hmf_bin, l);
      break;

    case HMF_S64:
      u64 = f->hmf_s64;
      for(i = 0; i < l; i++) {
	ptr[i] = u64;
	u64 = u64 >> 8;
      }
      break;
    }
    ptr += l;
  }
}


/**
 *
 */
int
htsmsg_binary_serialize(htsmsg_t *msg, void **datap, size_t *lenp, int maxlen)
{
  size_t len;
  uint8_t *data;

  len = htsmsg_binary_count(msg);
  if(len + 4 > maxlen)
    return -1;

  data = malloc(len + 4);

  data[0] = len >> 24;
  data[1] = len >> 16;
  data[2] = len >> 8;
  data[3] = len;

  htsmsg_binary_write(msg, data + 4);
  *datap = data;
  *lenp  = len + 4;
  return 0;
}


/**
 *
 */
int
htsmsg_binary_serialize_nolen(htsmsg_t *msg, void **datap, size_t *lenp,
                              int maxlen)
{
  size_t len;
  uint8_t *data;

  len = htsmsg_binary_count(msg);
  if(len > maxlen)
    return -1;

  data = malloc(len);
  htsmsg_binary_write(msg, data);
  *datap = data;
  *lenp  = len;
  return 0;
}
