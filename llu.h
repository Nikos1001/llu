
#ifndef LLU_H
#define LLU_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>

extern void* (*llu_realloc)(void* alloc, size_t oldSize, size_t newSize);
void* llu_malloc(size_t size);
void* llu_calloc(int cnt, size_t size);
void llu_free(void* alloc, size_t size);

#define llu_dynarr(type) (type*)((int*)llu_calloc(2, sizeof(int)) + 2)

#define llu_dynarrPush(arrPtr, val) do { \
        void* arr = (void*)(*(arrPtr)); \
        int* ptr = (int*)arr - 2; \
        ptr[0]++; \
        if(ptr[0] > ptr[1]) { \
            size_t oldSize = ptr[1] * sizeof(**(arrPtr)) + 2 * sizeof(int); \
            ptr[1] = ptr[1] == 0 ? 8 : 2 * ptr[1]; \
            size_t newSize = ptr[1] * sizeof(**(arrPtr)) + 2 * sizeof(int); \
            *(arrPtr) = (void*)llu_realloc(ptr, oldSize, newSize) + 2 * sizeof(int); \
        } \
        arr = (void*)(*(arrPtr)); \
        ptr = (int*)arr - 2; \
        (*(arrPtr))[ptr[0] - 1] = val; \
    } while(0);

#define llu_freeDynarr(arrPtr) do { \
        int* ptr = (int*)(*(arrPtr)) - 2; \
        size_t size = ptr[1] * sizeof(**(arrPtr)) + 2 * sizeof(int); \
        llu_free(ptr, size); \
        (*(arrPtr)) = NULL; \
    } while(0);


extern double llu_timerStart[256];
extern int llu_currTimer;

void llu_beginTimer();
float llu_endTimer();
void llu_profile(const char* name);

typedef struct {
    void* base;
    void* curr;
} llu_arena;

llu_arena* llu_makeSizedArena(size_t size);
llu_arena* llu_makeArena();
void llu_freeArena(llu_arena* arena);
void* llu_arenaPush(llu_arena* arena, size_t size);
#define llu_arenaAlloc(arena, type) llu_arenaPush(arena, sizeof(type))
#define llu_arenaAllocArray(arena, type, cnt) llu_arenaPush(arena, sizeof(type) * (cnt))
void llu_arenaPop(llu_arena* arena, void* ptr);
void llu_arenaClear(llu_arena* arena);

typedef struct llu_resourceHeader {
    int slot;
    int magic;
    struct llu_resourceHeader* next; 
} llu_resourceHeader;

typedef struct {
    llu_arena* arena;
    size_t resourceSize;
    int slots;
    int currMagic;
    llu_resourceHeader* firstFree;
    llu_resourceHeader* firstUsed;
} llu_resourcePool;

typedef struct {
    int slot;
    int magic;
    llu_resourcePool* pool;
} llu_handle;

llu_resourcePool* llu_makeResourcePool(size_t resourceSize);
void llu_freeResourcePool(llu_resourcePool* pool);
llu_handle llu_allocResource(llu_resourcePool* pool);
bool llu_verifyHandle(llu_handle handle);
void* llu_getResourcePtr(llu_handle handle); 
void llu_deallocResource(llu_handle handle);
llu_handle llu_nullHandle(llu_resourcePool* pool);
llu_handle llu_firstResource(llu_resourcePool* pool);
llu_handle llu_nextResource(llu_handle handle);

#define llu_stringify(...) #__VA_ARGS__
#define llu_call(funcPtr) if((funcPtr) != NULL) (funcPtr)

#define llu_arenaScope(arena) do { void* _scope_begin_ = (arena)->curr;
#define llu_arenaScopeEnd() (arena)->curr = _scope_begin_; } while(0);

typedef struct {
    int len;
    char* str;
} llu_str;

llu_str llu_makeString(char* str);

#ifdef LLU_IMPLEMENTATION

static void* llu_defaultRealloc(void* alloc, size_t oldSize, size_t newSize) {
    return realloc(alloc, newSize);
}

void* (*llu_realloc)(void* alloc, size_t oldSize, size_t newSize) = llu_defaultRealloc;

void* llu_malloc(size_t size) {
    return llu_realloc(NULL, 0, size);
}

void* llu_calloc(int cnt, size_t size) {
    void* alloc = llu_malloc(cnt * size); 
    memset(alloc, 0, cnt * size);
    return alloc;
}

void llu_free(void* alloc, size_t size) {
    llu_realloc(alloc, size, 0);
}

double llu_timerStart[256];
int llu_currTimer = 0;

static double currTime() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (double)t.tv_sec + (double)t.tv_usec / 1000000.0f;
}

void llu_beginTimer() {
    llu_timerStart[llu_currTimer] = currTime();
    llu_currTimer++;
}

float llu_endTimer() {
    llu_currTimer--;
    return currTime() - llu_timerStart[llu_currTimer]; 
}

void llu_profile(const char* name) {
    printf("%s: %gs\n", name, llu_endTimer());
}

llu_arena* llu_makeSizedArena(size_t size) {
    llu_arena* arena = (llu_arena*)malloc(sizeof(llu_arena));
    arena->base = malloc(size);
    arena->curr = arena->base;
    return arena;
}

llu_arena* llu_makeArena() {
    return llu_makeSizedArena((size_t)1024 * 1024 * 1024 * 1024);
}

void llu_freeArena(llu_arena* arena) {
    free(arena->base);
    free(arena);
}

void* llu_arenaPush(llu_arena* arena, size_t size) {

    // ensure size is a multiple of 8
    size += 7;
    size /= 8;
    size *= 8;

    arena->curr += size;
    return arena->curr - size;
}

void llu_arenaPop(llu_arena* arena, void* ptr) {
    arena->curr = ptr;
}

void llu_arenaClear(llu_arena* arena) {
    llu_arenaPop(arena, arena->base);
}



llu_resourcePool* llu_makeResourcePool(size_t resourceSize) {
    llu_resourcePool* pool = malloc(sizeof(llu_resourcePool));
    pool->arena = llu_makeArena(); 
    pool->currMagic = 0;
    pool->resourceSize = resourceSize;
    pool->slots = 0;
    pool->firstFree = NULL;
    pool->firstUsed = NULL;
    return pool;
}

void llu_freeResourcePool(llu_resourcePool* pool) {
    llu_freeArena(pool->arena);
    free(pool);
}

llu_handle llu_allocResource(llu_resourcePool* pool) {
    llu_resourceHeader* res;
    if(pool->firstFree != NULL) {
        res = pool->firstFree;
        pool->firstFree = res->next; 
    } else {
        res = llu_arenaPush(pool->arena, sizeof(llu_resourceHeader) + pool->resourceSize);
        res->slot = pool->slots;
        pool->slots++;
    }
    res->next = pool->firstUsed;
    pool->firstUsed = res;
    res->magic = pool->currMagic;
    pool->currMagic++;

    llu_handle handle;
    handle.magic = res->magic;
    handle.slot = res->slot;
    handle.pool = pool;
    
    return handle;
}

static llu_resourceHeader* llu_getHeader(llu_handle handle) {
    llu_resourceHeader* header = handle.pool->arena->base + handle.slot * (sizeof(llu_resourceHeader) + handle.pool->resourceSize);
    return header;
}

bool llu_verifyHandle(llu_handle handle) {
    llu_resourceHeader* header = llu_getHeader(handle); 
    return header->magic == handle.magic;
}

void* llu_getResourcePtr(llu_handle handle) {
    if(!llu_verifyHandle(handle)) {
        return NULL;
    }
    llu_resourceHeader* header = llu_getHeader(handle);
    return (void*)header + sizeof(llu_resourceHeader);
} 

void llu_deallocResource(llu_handle handle) {
    if(!llu_verifyHandle(handle)) {
        return;
    }
    llu_resourceHeader* header = llu_getHeader(handle); 
    if(header == handle.pool->firstUsed) {
        handle.pool->firstUsed = header->next;
    }
    header->magic = -1;
    header->next = handle.pool->firstFree;
    handle.pool->firstFree = header;
}

llu_handle llu_nullHandle(llu_resourcePool* pool) {
    llu_handle handle;
    handle.pool = pool;
    handle.magic = -1;
    handle.slot = 0;
    return handle;
}

llu_handle llu_firstResource(llu_resourcePool* pool) {
    if(pool->firstUsed == NULL)
        return llu_nullHandle(pool);
    llu_handle first;
    first.pool = pool;
    first.slot = pool->firstUsed->slot;
    first.magic = pool->firstUsed->magic;
    return first;
}

llu_handle llu_nextResource(llu_handle handle) {
    llu_resourceHeader* header = llu_getHeader(handle);
    llu_resourceHeader* nextHeader = header->next;
    if(nextHeader == NULL)
        return llu_nullHandle(handle.pool);
    llu_handle nextHandle;
    nextHandle.pool = handle.pool;
    nextHandle.slot = nextHeader->slot;
    nextHandle.magic = nextHeader->magic;
    return nextHandle;
}



llu_str llu_makeString(char* str) {
    llu_str s;
    s.len = strlen(str);
    s.str = str;
    return s;
}

#endif

#endif
