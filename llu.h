
#ifndef LLU_H
#define LLU_H

#include <stdlib.h>

typedef struct {
    void* base;
    void* curr;
} llu_arena;

llu_arena* llu_makeArena() {
    llu_arena* arena = (llu_arena*)malloc(sizeof(llu_arena));
    arena->base = malloc((size_t)1024 * 1024 * 1024 * 1024);
    arena->curr = arena->base;
    return arena;
}

void llu_freeArena(llu_arena* arena) {
    free(arena->base);
    free(arena);
}

void* llu_arenaAlloc(llu_arena* arena, size_t size) {

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

#endif
