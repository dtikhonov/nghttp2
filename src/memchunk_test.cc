/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "memchunk_test.h"

#include <CUnit/CUnit.h>

#include <nghttp2/nghttp2.h>

#include "memchunk.h"

namespace nghttp2 {

void test_pool_recycle(void) {
  MemchunkPool4K pool;

  CU_ASSERT(!pool.pool);
  CU_ASSERT(0 == pool.poolsize);
  CU_ASSERT(nullptr == pool.freelist);

  auto m1 = pool.get();

  CU_ASSERT(m1 == pool.pool.get());
  CU_ASSERT(MemchunkPool4K::value_type::size == pool.poolsize);
  CU_ASSERT(nullptr == pool.freelist);

  auto m2 = pool.get();

  CU_ASSERT(m2 == pool.pool.get());
  CU_ASSERT(2 * MemchunkPool4K::value_type::size == pool.poolsize);
  CU_ASSERT(nullptr == pool.freelist);
  CU_ASSERT(m1 == m2->knext.get());
  CU_ASSERT(nullptr == m1->knext.get());

  auto m3 = pool.get();

  CU_ASSERT(m3 == pool.pool.get());
  CU_ASSERT(3 * MemchunkPool4K::value_type::size == pool.poolsize);
  CU_ASSERT(nullptr == pool.freelist);

  pool.recycle(m3);

  CU_ASSERT(m3 == pool.pool.get());
  CU_ASSERT(3 * MemchunkPool4K::value_type::size == pool.poolsize);
  CU_ASSERT(m3 == pool.freelist);

  auto m4 = pool.get();

  CU_ASSERT(m3 == m4);
  CU_ASSERT(m4 == pool.pool.get());
  CU_ASSERT(3 * MemchunkPool4K::value_type::size == pool.poolsize);
  CU_ASSERT(nullptr == pool.freelist);

  pool.recycle(m2);
  pool.recycle(m1);

  CU_ASSERT(m1 == pool.freelist);
  CU_ASSERT(m2 == m1->next);
  CU_ASSERT(nullptr == m2->next);
}

using Memchunk16 = Memchunk<16>;
using MemchunkPool16 = Pool<Memchunk16>;
using Memchunks16 = Memchunks<Memchunk16>;

void test_memchunks_append(void) {
  MemchunkPool16 pool;
  Memchunks16 chunks(&pool);

  chunks.append_cstr("012");

  auto m = chunks.tail;

  CU_ASSERT(3 == m->len());
  CU_ASSERT(13 == m->left());

  chunks.append_cstr("3456789abcdef@");

  CU_ASSERT(16 == m->len());
  CU_ASSERT(0 == m->left());

  m = chunks.tail;

  CU_ASSERT(1 == m->len());
  CU_ASSERT(15 == m->left());
  CU_ASSERT(17 == chunks.rleft());

  char buf[16];
  size_t nread;

  nread = chunks.remove(buf, 8);

  CU_ASSERT(8 == nread);
  CU_ASSERT(0 == memcmp("01234567", buf, nread));
  CU_ASSERT(9 == chunks.rleft());

  nread = chunks.remove(buf, sizeof(buf));

  CU_ASSERT(9 == nread);
  CU_ASSERT(0 == memcmp("89abcdef@", buf, nread));
  CU_ASSERT(0 == chunks.rleft());
  CU_ASSERT(nullptr == chunks.head);
  CU_ASSERT(nullptr == chunks.tail);
  CU_ASSERT(32 == pool.poolsize);
}

void test_memchunks_drain(void) {
  MemchunkPool16 pool;
  Memchunks16 chunks(&pool);

  chunks.append_cstr("0123456789");

  size_t nread;

  nread = chunks.drain(3);

  CU_ASSERT(3 == nread);

  char buf[16];

  nread = chunks.remove(buf, sizeof(buf));

  CU_ASSERT(7 == nread);
  CU_ASSERT(0 == memcmp("3456789", buf, nread));
}

void test_memchunks_riovec(void) {
  MemchunkPool16 pool;
  Memchunks16 chunks(&pool);

  char buf[3 * 16];

  chunks.append(buf, sizeof(buf));

  struct iovec iov[2];
  auto iovcnt = chunks.riovec(iov, util::array_size(iov));

  auto m = chunks.head;

  CU_ASSERT(2 == iovcnt);
  CU_ASSERT(m->begin == iov[0].iov_base);
  CU_ASSERT(m->len() == iov[0].iov_len);

  m = m->next;

  CU_ASSERT(m->begin == iov[1].iov_base);
  CU_ASSERT(m->len() == iov[1].iov_len);

  chunks.drain(2 * 16);

  iovcnt = chunks.riovec(iov, util::array_size(iov));

  CU_ASSERT(1 == iovcnt);

  m = chunks.head;
  CU_ASSERT(m->begin == iov[0].iov_base);
  CU_ASSERT(m->len() == iov[0].iov_len);
}

void test_memchunks_recycle(void) {
  MemchunkPool16 pool;
  {
    Memchunks16 chunks(&pool);
    char buf[32];
    chunks.append(buf, sizeof(buf));
  }
  CU_ASSERT(32 == pool.poolsize);
  CU_ASSERT(nullptr != pool.freelist);

  auto m = pool.freelist;
  m = m->next;

  CU_ASSERT(nullptr != m);
  CU_ASSERT(nullptr == m->next);
}

} // namespace nghttp2