
#ifndef LLU_H
#define LLU_H

#include <stdlib.h>
#include <stdbool.h>

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

#define llu_stringify(...) #__VA_ARGS__

#define llu_arenaScope(arena) do { void* _scope_begin_ = (arena)->curr;
#define llu_arenaScopeEnd() (arena)->curr = _scope_begin_; } while(0);

typedef struct {
    int len;
    char* str;
} llu_str;

llu_str llu_makeString(const char* str);
llu_str llu_readFile(llu_arena* arena, llu_str path);
llu_str llu_joinPath(llu_arena* arena, llu_str p1, llu_str p2);

#ifdef LLU_IMPLEMENTATION

#include <sys/time.h>
#include <stdio.h>

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
    res->next = NULL;
    res->magic = pool->currMagic;
    pool->currMagic++;

    llu_handle handle;
    handle.magic = res->magic;
    handle.slot = res->slot;
    handle.pool = pool;
    
    return handle;
}

bool llu_verifyHandle(llu_handle handle) {
    llu_resourceHeader* header = handle.pool->arena->base + handle.slot * (sizeof(llu_resourceHeader) + handle.pool->resourceSize);
    return header->magic == handle.magic;
}

void* llu_getResourcePtr(llu_handle handle) {
    if(!llu_verifyHandle(handle)) {
        return NULL;
    }
    llu_resourceHeader* header = handle.pool->arena->base + handle.slot * (sizeof(llu_resourceHeader) + handle.pool->resourceSize);
    return (void*)header + sizeof(llu_resourceHeader);
} 

void llu_deallocResource(llu_handle handle) {
    if(!llu_verifyHandle(handle)) {
        return;
    }
    llu_resourceHeader* header = handle.pool->arena->base + handle.slot * (sizeof(llu_resourceHeader) + handle.pool->resourceSize);
    header->magic = -1;
    header->next = handle.pool->firstFree;
    handle.pool->firstFree = header;
}

#include <string.h>

llu_str llu_makeString(const char* str) {
    llu_str s;
    s.len = strlen(str);
    s.str = str;
    return s;
}

llu_str llu_readFile(llu_arena* arena, llu_str path) {
    // TODO: implement virtual file system & single exec application support
    FILE* f = fopen(path.str, "rb");
    if(f == NULL)
        return (llu_str){0};
    fseek(f, 0L, SEEK_END);
    size_t fileSize = ftell(f);
    rewind(f);
    char* buffer = llu_arenaPush(arena, fileSize + 1);
    fread(buffer, sizeof(char), fileSize, f);
    buffer[fileSize] = '\0';
    fclose(f);
    
    llu_str res;
    res.len = fileSize;
    res.str = buffer;

    return res; 
}

llu_str llu_joinPath(llu_arena* arena, llu_str p1, llu_str p2) {
    int resLen = p1.len + p2.len;
    bool addInfixSlash = false;
    if(p1.len > 0 && p1.str[p1.len - 1] != '/') {
        addInfixSlash = true;
        resLen++;
    }
    llu_str res;
    res.len = resLen;
    res.str = llu_arenaPush(arena, resLen + 1);
    memcpy(res.str, p1.str, p1.len);
    memcpy(res.str + resLen - p2.len, p2.str, p2.len);
    res.str[resLen] = '\0';
    if(addInfixSlash) {
        res.str[p1.len] = '/';
    }
    return res;
}

#endif

#endif
