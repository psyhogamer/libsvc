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

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <inttypes.h>
#include <sys/queue.h>

TAILQ_HEAD(mbuf_data_queue, mbuf_data);


typedef struct mbuf {
  struct mbuf_data_queue mq_buffers;
  size_t mq_size;
  size_t mq_alloc_size;
} mbuf_t;

void mbuf_init(mbuf_t *m);

void mbuf_set_chunk_size(mbuf_t *m, size_t s);

void mbuf_clear(mbuf_t *m);

void mbuf_vqprintf(mbuf_t *m, const char *fmt, va_list ap);

void mbuf_qprintf(mbuf_t *m, const char *fmt, ...);

void mbuf_append(mbuf_t *m, const void *buf, size_t len);

void mbuf_append_prealloc(mbuf_t *m, void *buf, size_t len);

size_t mbuf_read(mbuf_t *m, void *buf, size_t len);

size_t mbuf_peek(mbuf_t *m, void *buf, size_t len);

size_t mbuf_peek_tail(mbuf_t *mq, void *buf, size_t len);

size_t mbuf_drop(mbuf_t *m, size_t len);

int mbuf_find(mbuf_t *m, uint8_t v);

void mbuf_appendq(mbuf_t *m, mbuf_t *src);

void mbuf_append_and_escape_xml(mbuf_t *m, const char *str);

void mbuf_append_and_escape_url(mbuf_t *m, const char *s);

void mbuf_append_and_escape_jsonstr(mbuf_t *m, const char *s);

void mbuf_dump_raw_stderr(mbuf_t *m);

const void *mbuf_pullup(mbuf_t *mq, size_t bytes);

char *mbuf_clear_to_string(mbuf_t *mq);