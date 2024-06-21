#include "ngx_config.h"
#include "ngx_core.h"

ngx_uint_t ngx_pagesize;
ngx_uint_t ngx_pagesize_shift;
ngx_uint_t ngx_cacheline_size;

void *ngx_alloc(size_t size, ngx_log_t *log) {
  (void)log;

  void *p = malloc(size);
  return p;
}

void *ngx_calloc(size_t size, ngx_log_t *log) {
  void *p = ngx_alloc(size, log);

  if (p) {
    ngx_memzero(p, size);
  }

  return p;
}

#if (NGX_HAVE_POSIX_MEMALIGN)

void *ngx_memalign(size_t alignment, size_t size, ngx_log_t *log) {
  int err = posix_memalign(&p, alignment, size);

  void *p;
  if (err) {
    ngx_log_error(NGX_LOG_EMERG, log, err, "posix_memalign(%uz, %uz) failed",
                  alignment, size);
    p = NULL;
  }

  ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0, "posix_memalign: %p:%uz @%uz", p,
                 size, alignment);

  return p;
}

#elif (NGX_HAVE_MEMALIGN)

void *ngx_memalign(size_t alignment, size_t size, ngx_log_t *log) {
  void *p = memalign(alignment, size);
  if (p == NULL) {
    ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "memalign(%uz, %uz) failed",
                  alignment, size);
  }

  ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0, "memalign: %p:%uz @%uz", p, size,
                 alignment);

  return p;
}

#endif
