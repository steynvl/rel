#include <assert.h>
#include "transition.h"
#include "hashset.h"

typedef struct BuildState BuildState;

#define SIZE 1024

static BuildState *build_state;

static Prog* create_prog()
{
    Prog *prog;

    prog = umal(sizeof(Prog));
    prog->start = umal(SIZE * sizeof(Inst));
    prog->len = SIZE;

    return prog;
}

struct BuildState {
    Inst *pc;
};

static void make_match(Inst **pc)
{
    (*pc)->opcode = Match;
    (*pc)++;
}

static void make_char(Inst **pc, int ch)
{
    (*pc)->opcode = Char;
    (*pc)->c = ch;
    (*pc)++;
}

static void make_star(Inst **pc, int ch)
{
    Inst *p1;

    (*pc)->opcode = Split;
    p1 = (*pc)++;

    p1->x = *pc;
    (*pc)->opcode = Char;
    (*pc)->c = ch;
    (*pc)++;

    (*pc)->opcode = Jmp;
    (*pc)->x = p1;
    (*pc)++;

    p1->y = *pc;
}

static void build(Prog *prog, HashSet *final_states,
                  GHashTable *state_map, GHashTable *moves, gpointer move_key)
{
    Inst *p1, *p2;
    StateAndTransitionLabel *s;
    GPtrArray *ptr_arr, *instructions;
    gpointer key, value;
    int i, idx;
    gboolean is_final;

    assert(!g_hash_table_contains(state_map, move_key));

    idx = build_state->pc - prog->start;
    g_hash_table_insert(state_map, move_key, GINT_TO_POINTER(idx));

    is_final = hash_set_contains(final_states, move_key);

    value = g_hash_table_lookup(moves, move_key);
    if (value == nil) {
        assert(is_final);
        make_match(&build_state->pc);
        return;
    }

    assert(value != nil);
    ptr_arr = hash_set_to_g_ptr_array(value);

    if (ptr_arr->len == 1) {
        s = g_ptr_array_index(ptr_arr, 0);
        key = GINT_TO_POINTER(s->state);

        if (is_final) assert(GPOINTER_TO_INT(move_key) == s->state);

        if (GPOINTER_TO_INT(move_key) == s->state) {
            make_star(&build_state->pc, s->tl->info);
        } else {
            make_char(&build_state->pc, s->tl->info);

            value = g_hash_table_lookup(state_map, key);
            if (value == nil) {
                build(prog, final_states, state_map, moves, key);
                assert(g_hash_table_lookup(state_map, key) != nil);
            } else if (build_state->pc - prog->start != GPOINTER_TO_INT(value) + 1) {
                p1 = build_state->pc++;
                p1->opcode = Jmp;
                p1->x = prog->start + GPOINTER_TO_INT(value);
            }
        }
    } else {
        instructions = g_ptr_array_new();
        for (i = 0; i < ptr_arr->len - 1; i++) {
            build_state->pc->opcode = Split;
            build_state->pc->x = build_state->pc + 1;
            build_state->pc->y = nil;
            g_ptr_array_add(instructions, build_state->pc);
            build_state->pc++;
        }

        s = g_ptr_array_index(ptr_arr, 0);
        make_char(&build_state->pc, s->tl->info);
        key = GINT_TO_POINTER(s->state);

        value = g_hash_table_lookup(state_map, key);
        if (value == nil) {
            build(prog, final_states, state_map, moves, key);
            value = g_hash_table_lookup(state_map, key);
            assert(value != nil);
        } else if (build_state->pc - prog->start != GPOINTER_TO_INT(value) + 1) {
            p1 = build_state->pc++;
            p1->opcode = Jmp;
            p1->x = prog->start + GPOINTER_TO_INT(value);
        }

        for (i = ptr_arr->len - 1; i > 0; i--) {
            s = g_ptr_array_index(ptr_arr, i);
            p1 = (Inst*) g_ptr_array_index(instructions, i - 1);
            key = GINT_TO_POINTER(s->state);
            value = g_hash_table_lookup(state_map, key);
            if (value == nil) {
                build(prog, final_states, state_map, moves, key);
                value = g_hash_table_lookup(state_map, key);
                assert(value != nil);
            }
            make_char(&build_state->pc, s->tl->info);
            p2 = build_state->pc++;
            p2->opcode = Jmp;
            p2->x = prog->start + GPOINTER_TO_INT(value);
            p1->y = p2 - 1;
        }
    }

    if (is_final)
        make_match(&build_state->pc);
}

Prog *build_prog(GHashTable *moves, HashSet *final_states)
{
    Prog *prog;
    HashSet *hs;
    HashSetIter iter;
    StateAndTransitionLabel *s;
    GHashTable *state_map;
    GHashTableIter ht_iter;
    gpointer key, val;
    int i;

    printf("--- IN BUILDPROG ---\n");
    printf("final_states: ");
    hash_set_iter_init(&iter, final_states);
    while (hash_set_iter_next(&iter, &val)) {
        printf("%d ", GPOINTER_TO_INT(val));
    }
    printf("\n");

    g_hash_table_iter_init(&ht_iter, moves);
    while (g_hash_table_iter_next(&ht_iter, &key, &val)) {
        printf("%d -> [", GPOINTER_TO_INT(key));
        hs = (HashSet*) val;
        hash_set_iter_init(&iter, hs);
        while (hash_set_iter_next(&iter, &val)) {
            s = (StateAndTransitionLabel*) val;
            printf("(%d, %c) ", s->state, s->tl->info);
        }
        printf("]\n");
    }

    assert(g_hash_table_size(moves) > 0);

    state_map = g_hash_table_new(g_direct_hash, g_direct_equal);
    prog = create_prog();
    build_state = umal(sizeof(BuildState));
    build_state->pc = prog->start;

    build(prog, final_states, state_map, moves, GINT_TO_POINTER(0));

    prog->len = build_state->pc - prog->start;
    printf("prog->len = %d\n", prog->len);
    for (i = 0; i < prog->len; i++)
        prog->start[i].gen = 0;

    return prog;
}
