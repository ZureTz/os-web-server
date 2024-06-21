#include <pthread.h>
#include <string.h>

#include "ngx_alloc.h"
#include "ngx_config.h"
#include "ngx_core.h"
#include "ngx_palloc.h"

static ngx_inline void *ngx_palloc_small(ngx_pool_t *pool, size_t size,
                                         ngx_uint_t align);
static void *ngx_palloc_block(ngx_pool_t *pool, size_t size);
static void *ngx_palloc_large(ngx_pool_t *pool, size_t size);

ngx_pool_t *ngx_create_pool(size_t size, ngx_log_t *log) {
  ngx_pool_t *p = ngx_memalign(NGX_POOL_ALIGNMENT, size, log);
  if (p == NULL) {
    return NULL;
  }

  p->d.last = (u_char *)p + sizeof(ngx_pool_t);
  p->d.end = (u_char *)p + size;
  p->d.next = NULL;
  p->d.failed = 0;

  size = size - sizeof(ngx_pool_t);
  p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;

  p->current = p;
  p->chain = NULL;
  p->large = NULL;
  p->cleanup = NULL;
  p->log = log;
  p->reference_count = 0;

  return p;
}

void ngx_destroy_pool(ngx_pool_t *pool) {
  for (ngx_pool_cleanup_t *c = pool->cleanup; c; c = c->next) {
    if (c->handler) {
      c->handler(c->data);
    }
  }

#if (NGX_DEBUG)

  /*
   * we could allocate the pool->log from this pool
   * so we cannot use this log while free()ing the pool
   */

  for (ngx_pool_large_t *l = pool->large; l; l = l->next) {
    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0, "free: %p", l->alloc);
  }

  for (ngx_pool_t *p = pool, *n = pool->d.next; /* void */;
       p = n, n = n->d.next) {
    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, pool->log, 0, "free: %p, unused: %uz",
                   p, p->d.end - p->d.last);

    if (n == NULL) {
      break;
    }
  }

#endif

  for (ngx_pool_large_t *l = pool->large; l; l = l->next) {
    if (l->alloc) {
      ngx_free(l->alloc);
    }
  }

  for (ngx_pool_t *p = pool, *n = pool->d.next; /* void */;
       p = n, n = n->d.next) {
    ngx_free(p);
    if (n == NULL) {
      break;
    }
  }
}

void ngx_reset_pool(ngx_pool_t *pool) {
  for (ngx_pool_large_t *l = pool->large; l; l = l->next) {
    if (l->alloc) {
      ngx_free(l->alloc);
    }
  }

  for (ngx_pool_t *p = pool; p; p = p->d.next) {
    p->d.last = (u_char *)p + sizeof(ngx_pool_t);
    p->d.failed = 0;
  }

  pool->current = pool;
  pool->chain = NULL;
  pool->large = NULL;
}

void *ngx_palloc(ngx_pool_t *pool, size_t size) {
#if !(NGX_DEBUG_PALLOC)
  if (size <= pool->max) {
    return ngx_palloc_small(pool, size, 1);
  }
#endif
  return ngx_palloc_large(pool, size);
}

void *ngx_pnalloc(ngx_pool_t *pool, size_t size) {
#if !(NGX_DEBUG_PALLOC)
  if (size <= pool->max) {
    return ngx_palloc_small(pool, size, 0);
  }
#endif

  return ngx_palloc_large(pool, size);
}

static ngx_inline void *ngx_palloc_small(ngx_pool_t *pool, size_t size,
                                         ngx_uint_t align) {
  ngx_pool_t *p = pool->current;

  do {
    u_char *m = p->d.last;
    if (align) {
      m = ngx_align_ptr(m, NGX_ALIGNMENT);
    }
    if ((size_t)(p->d.end - m) >= size) {
      p->d.last = m + size;
      return m;
    }
    p = p->d.next;
  } while (p);

  return ngx_palloc_block(pool, size);
}

static void *ngx_palloc_block(ngx_pool_t *pool, size_t size) {
  size_t psize = (size_t)(pool->d.end - (u_char *)pool);
  u_char *m = ngx_memalign(NGX_POOL_ALIGNMENT, psize, pool->log);

  if (m == NULL) {
    return NULL;
  }

  ngx_pool_t *new = (ngx_pool_t *)m;

  new->d.end = m + psize;
  new->d.next = NULL;
  new->d.failed = 0;

  m += sizeof(ngx_pool_data_t);
  m = ngx_align_ptr(m, NGX_ALIGNMENT);
  new->d.last = m + size;

  ngx_pool_t *p;
  for (p = pool->current; p->d.next; p = p->d.next) {
    if (p->d.failed++ > 4) {
      pool->current = p->d.next;
    }
  }
  p->d.next = new;

  return m;
}

static void *ngx_palloc_large(ngx_pool_t *pool, size_t size) {

  void *p = ngx_alloc(size, pool->log);
  if (p == NULL) {
    return NULL;
  }

  ngx_uint_t n = 0;

  ngx_pool_large_t *large;
  for (large = pool->large; large; large = large->next) {
    if (large->alloc == NULL) {
      large->alloc = p;
      return p;
    }

    if (n++ > 3) {
      break;
    }
  }

  large = ngx_palloc_small(pool, sizeof(ngx_pool_large_t), 1);
  if (large == NULL) {
    ngx_free(p);
    return NULL;
  }

  large->alloc = p;
  large->next = pool->large;
  pool->large = large;

  return p;
}

void *ngx_pmemalign(ngx_pool_t *pool, size_t size, size_t alignment) {
  (void)alignment;

  void *p = ngx_memalign(alignment, size, pool->log);
  if (p == NULL) {
    return NULL;
  }

  ngx_pool_large_t *large = ngx_palloc_small(pool, sizeof(ngx_pool_large_t), 1);
  if (large == NULL) {
    ngx_free(p);
    return NULL;
  }

  large->alloc = p;
  large->next = pool->large;
  pool->large = large;

  return p;
}

ngx_int_t ngx_pfree(ngx_pool_t *pool, void *p) {
  for (ngx_pool_large_t *l = pool->large; l; l = l->next) {
    if (p == l->alloc) {
      ngx_free(l->alloc);
      l->alloc = NULL;
      return NGX_OK;
    }
  }
  return NGX_DECLINED;
}

void *ngx_pcalloc(ngx_pool_t *pool, size_t size) {
  void *p = ngx_palloc(pool, size);
  if (p) {
    ngx_memzero(p, size);
  }
  return p;
}