#ifndef LIBFLUX_H
#define LIBFLUX_H

#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
    #define FLUX_WINDOWS 1
    #include <windows.h>
#else
    #define FLUX_POSIX 1
    #include <pthread.h>
#endif

#define FLUX_MAX_DEPTH 64
#define FLUX_POOL_SIZE 2048
#define FLUX_ERROR_MSG_MAX 512

typedef struct flux_scope flux_scope_t;
typedef struct flux_error flux_error_t;
typedef struct flux_guard flux_guard_t;
typedef struct flux_tls flux_tls_t;

struct flux_error {
    int32_t code;
    char msg[FLUX_ERROR_MSG_MAX];
    char file[64];
    int line;
};

struct flux_guard {
    void (*dtor)(void*);
    void* ptr;
    flux_guard_t* next;
};

struct flux_scope {
    jmp_buf buf;
    flux_error_t err;
    flux_guard_t* guards;
    bool active;
};

typedef struct flux_pool {
    flux_guard_t guards[FLUX_POOL_SIZE];
    atomic_int idx;
} flux_pool_t;

#if FLUX_WINDOWS
    typedef DWORD flux_tls_key_t;
#else
    typedef pthread_key_t flux_tls_key_t;
#endif

static flux_tls_key_t __flux_tls_key;
static atomic_bool __flux_initialized = false;

static inline void __flux_tls_destructor(void* ptr);
static inline flux_tls_t* __flux_get_tls(void);

struct flux_tls {
    flux_scope_t stack[FLUX_MAX_DEPTH];
    int top;
    flux_pool_t pool;
};

static inline void __flux_init_once(void) {
    if (atomic_load(&__flux_initialized)) return;
    static bool in_init = false;
    if (in_init) abort();
    in_init = true;

#if FLUX_WINDOWS
    __flux_tls_key = TlsAlloc();
    if (__flux_tls_key == TLS_OUT_OF_INDEXES) abort();
#else
    if (pthread_key_create(&__flux_tls_key, __flux_tls_destructor) != 0) abort();
#endif

    atomic_store(&__flux_initialized, true);
    in_init = false;
}

static inline void __flux_tls_destructor(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

static inline flux_tls_t* __flux_get_tls(void) {
    __flux_init_once();
#if FLUX_WINDOWS
    flux_tls_t* tls = (flux_tls_t*)TlsGetValue(__flux_tls_key);
    if (GetLastError() != ERROR_SUCCESS) abort();
#else
    flux_tls_t* tls = (flux_tls_t*)pthread_getspecific(__flux_tls_key);
#endif
    if (!tls) {
        tls = (flux_tls_t*)calloc(1, sizeof(flux_tls_t));
        if (!tls) abort();
#if FLUX_WINDOWS
        if (!TlsSetValue(__flux_tls_key, tls)) abort();
#else
        if (pthread_setspecific(__flux_tls_key, tls) != 0) abort();
#endif
    }
    return tls;
}

static inline flux_guard_t* __flux_acquire_guard(flux_pool_t* pool) {
    int idx = atomic_fetch_add(&pool->idx, 1);
    if (idx >= FLUX_POOL_SIZE) {
        atomic_fetch_sub(&pool->idx, 1);
        return NULL;
    }
    return &pool->guards[idx];
}

static inline void __flux_reset_pool(flux_pool_t* pool) {
    atomic_store(&pool->idx, 0);
}

static inline void __flux_release_guards(flux_guard_t* head) {
    while (head) {
        if (head->dtor && head->ptr) {
            head->dtor(head->ptr);
        }
        head = head->next;
    }
}

static inline flux_error_t __flux_make_error(int32_t code, const char* msg, const char* file, int line) {
    flux_error_t e = {0};
    e.code = code;
    if (msg) {
        strncpy(e.msg, msg, FLUX_ERROR_MSG_MAX - 1);
        e.msg[FLUX_ERROR_MSG_MAX - 1] = '\0';
    }
    if (file) {
        const char* basename = strrchr(file, '/');
        if (!basename) basename = strrchr(file, '\\');
        basename = basename ? basename + 1 : file;
        strncpy(e.file, basename, sizeof(e.file) - 1);
        e.file[sizeof(e.file) - 1] = '\0';
    }
    e.line = line;
    return e;
}

static inline void flux_error_print(const flux_error_t* e) {
    if (e && e->msg[0]) {
        fprintf(stderr, "🔥 [%s:%d] ERR %d: %s\n", e->file, e->line, (int)e->code, e->msg);
    }
}

#define FLUX_THROW(code, msg) do { \
    flux_tls_t* __tls = __flux_get_tls(); \
    if (__tls->top >= 0 && __tls->top < FLUX_MAX_DEPTH) { \
        __tls->stack[__tls->top].err = __flux_make_error(code, msg, __FILE__, __LINE__); \
        longjmp(__tls->stack[__tls->top].buf, 1); \
    } else { \
        flux_error_t __e = __flux_make_error(code, msg, __FILE__, __LINE__); \
        flux_error_print(&__e); \
        abort(); \
    } \
} while(0)

#define FLUX_THROW_ERRNO(msg) do { \
    char __buf[FLUX_ERROR_MSG_MAX]; \
    strerror_r(errno, __buf, sizeof(__buf)); \
    char __full_msg[FLUX_ERROR_MSG_MAX]; \
    snprintf(__full_msg, sizeof(__full_msg), "%s: %s", msg, __buf); \
    FLUX_THROW(errno, __full_msg); \
} while(0)

#define FLUX_THROW_FILE(msg)     FLUX_THROW(1, msg)
#define FLUX_THROW_MEMORY()      FLUX_THROW(2, "out of memory")
#define FLUX_THROW_PARSE(msg)    FLUX_THROW(3, msg)
#define FLUX_THROW_INVALID(msg)  FLUX_THROW(4, msg)
#define FLUX_THROW_LIMIT()       FLUX_THROW(5, "resource limit exceeded")

#define FLUX_TRY \
    do { \
        flux_tls_t* __tls = __flux_get_tls(); \
        if (__tls->top + 1 >= FLUX_MAX_DEPTH) abort(); \
        int __l = ++__tls->top; \
        __tls->stack[__l].guards = NULL; \
        __tls->stack[__l].active = true; \
        __flux_reset_pool(&__tls->pool); \
        if (setjmp(__tls->stack[__l].buf) == 0) {

#define FLUX_CATCH(e) \
            __tls->stack[__l].active = false; \
            __tls->top--; \
        } else { \
            flux_error_t* e = &__tls->stack[__l].err; \
            __flux_release_guards(__tls->stack[__l].guards); \
            __tls->stack[__l].active = false; \
            __tls->top--;

#define FLUX_END_TRY \
        } \
    } while(0)

#define FLUX_DEFER(dt, p) do { \
    flux_tls_t* __tls = __flux_get_tls(); \
    flux_guard_t* __g = __flux_acquire_guard(&__tls->pool); \
    if (__g) { \
        __g->dtor = (void(*)(void*))(dt); \
        __g->ptr = (void*)(p); \
        __g->next = __tls->stack[__tls->top].guards; \
        __tls->stack[__tls->top].guards = __g; \
    } else { \
        FLUX_THROW_LIMIT(); \
    } \
} while(0)

#define FLUX_MALLOC(sz) ({ \
    size_t __sz = (sz); \
    if (__sz == 0) __sz = 1; \
    void* __p = malloc(__sz); \
    if (!__p) FLUX_THROW_MEMORY(); \
    FLUX_DEFER(free, __p); \
    __p; \
})

#define FLUX_CALLOC(nmemb, sz) ({ \
    size_t __nmemb = (nmemb); \
    size_t __sz = (sz); \
    if (__nmemb == 0 || __sz == 0) __nmemb = __sz = 1; \
    void* __p = calloc(__nmemb, __sz); \
    if (!__p) FLUX_THROW_MEMORY(); \
    FLUX_DEFER(free, __p); \
    __p; \
})

#define FLUX_REALLOC(ptr, new_sz) ({ \
    void* __old = (ptr); \
    size_t __sz = (new_sz); \
    if (__sz == 0) __sz = 1; \
    void* __p = realloc(__old, __sz); \
    if (!__p) FLUX_THROW_MEMORY(); \
    FLUX_DEFER(free, __p); \
    __p; \
})

#define FLUX_STRDUP(s) ({ \
    const char* __s = (s); \
    char* __p = __s ? strdup(__s) : NULL; \
    if (!__p && __s) FLUX_THROW_MEMORY(); \
    if (__p) FLUX_DEFER(free, __p); \
    __p; \
})

#define FLUX_FOPEN(path, mode) ({ \
    const char* __path = (path); \
    const char* __mode = (mode); \
    FILE* __f = fopen(__path, __mode); \
    if (!__f) { \
        char __msg[256]; \
        snprintf(__msg, sizeof(__msg), "fopen('%s', '%s') failed", __path, __mode); \
        FLUX_THROW_FILE(__msg); \
    } \
    FLUX_DEFER(fclose, __f); \
    __f; \
})

#define FLUX_CLOSE_FD(fd) do { \
    int __fd = (fd); \
    if (__fd >= 0) { \
        FLUX_DEFER(close, (void*)(intptr_t)__fd); \
    } \
} while(0)

#if FLUX_POSIX
    #include <unistd.h>
    #define close close
#endif

#endif /* LIBFLUX_H */