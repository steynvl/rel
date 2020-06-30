#include <assert.h>
#include "transition.h"
#include "hashset.h"

Prog *build_prog(GHashTable *moves, HashSet *final_states);

#define DEBUG 1

void add_sl_transition(TransitionTable *tt, StateList *from,
                       StateList *to, TransitionLabel *label)
{
    GHashTable *ht;
    GSList *to_states;
    gpointer value;

    value = g_hash_table_lookup(tt->ht, from);
    if (value == NULL) {
        ht = g_hash_table_new(transition_label_hash, transition_label_eq);
        g_hash_table_insert(tt->ht, from, ht);
    } else {
        ht = (GHashTable*) value;
    }

    value = g_hash_table_lookup(ht, label);
    if (value == NULL) {
        to_states = NULL;
        to_states = g_slist_prepend(to_states, to);
        g_hash_table_insert(ht, label, to_states);
    } else {
        value = g_slist_prepend(value, to);
        g_hash_table_replace(ht, label, value);
    }
}

static void add_tl_transition(GHashTable *ht, TransitionLabel *key, StateList *to)
{
    gpointer value;
    HashSet *to_states;

    value = g_hash_table_lookup(ht, key);
    if (value == NULL) {
        to_states = hash_set_new(state_list_hash, state_list_eq);
        hash_set_add(to_states, to);
        g_hash_table_insert(ht, key, to_states);
    } else {
        to_states = (HashSet*) value;
        if (!hash_set_add(to_states, to)) {
            g_hash_table_replace(ht, key, to_states);
        }
    }
}

static void
visit_forward(TransitionTable *tt_from, StateList *sl, HashSet *reached)
{
    GHashTable *ht;
    GHashTableIter iter;
    gpointer key, value;
    GSList *to_states;

    if (hash_set_contains(reached, sl))
        return;

    hash_set_add(reached, sl);
    value = g_hash_table_lookup(tt_from->ht, sl);
    if (value == NULL)
        return;

    ht = (GHashTable*) value;

    g_hash_table_iter_init(&iter, ht);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        to_states = (GSList*) value;
        while (to_states != NULL) {
            visit_forward(tt_from, to_states->data, reached);
            to_states = g_slist_next(to_states);
        }
    }
}

static HashSet*
get_reachable_states_from(TransitionTable *tt_from, StateList *sl)
{
    HashSet *result;

    result = hash_set_new(state_list_hash, state_list_eq);
    visit_forward(tt_from, sl, result);
    return result;
}

static HashSet*
get_reaching_states(TransitionTable *tt_to, StateList *sl)
{
    return get_reachable_states_from(tt_to, sl);
}

static HashSet*
get_alive_states(TransitionTable *tt_from, TransitionTable *tt_to,
                 StateList *is, StateList *fs)
{
    HashSet *alive_states, *reachable_from_init, *reaching_final;
    HashSetIter iter;
    gpointer key;

    reachable_from_init = get_reachable_states_from(tt_from, is);
    reaching_final = get_reaching_states(tt_to, fs);

    alive_states = hash_set_new(state_list_hash, state_list_eq);

    hash_set_iter_init(&iter, reachable_from_init);
    while (hash_set_iter_next(&iter, &key)) {
        if (hash_set_contains(reaching_final, key)) {
            hash_set_add(alive_states, key);
        }
    }

    return alive_states;
}

static CollectionStateList*
eps_closure(TransitionTable *tt, StateList *curr_state, HashSet *alive_states)
{
    CollectionStateList *ret;
    HashSet *reached;
    StateList *sl, *cs;
    TransitionLabel *tl;
    GQueue *to_visit;
    GHashTableIter iter;
    GHashTable *ht;
    GSList *to_states;
    gpointer key, value;

    to_visit = g_queue_new();
    reached = hash_set_new(state_list_hash, state_list_eq);

    g_queue_push_head(to_visit, curr_state);
    hash_set_add(reached, curr_state);

    while (!g_queue_is_empty(to_visit)) {
        sl = g_queue_pop_head(to_visit);

        value = g_hash_table_lookup(tt->ht, sl);
        if (value == NULL)
            continue;

        ht = (GHashTable*) value;

        g_hash_table_iter_init(&iter, ht);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            tl = (TransitionLabel*) key;
            if (tl->label == Epsilon) {
                to_states = (GSList*) value;
                while (to_states != NULL) {
                    cs = (StateList*) to_states->data;
                    if (hash_set_contains(alive_states, cs) && !hash_set_contains(reached, cs)) {
                        g_queue_push_head(to_visit, cs);
                        hash_set_add(reached, cs);
                    }
                    to_states = g_slist_next(to_states);
                }
            }

        }
    }

    ret = umal(sizeof(CollectionStateList));
    ret->state_sets = reached;

    return ret;
}

static CollectionStateList*
collec_sl_eps_closure(TransitionTable *tt, HashSet *states, HashSet *alive_states)
{
    CollectionStateList *ret;
    HashSetIter iter;
    StateList *sl;
    gpointer val;

    ret = umal(sizeof(CollectionStateList));
    ret->state_sets = hash_set_new(state_list_hash, state_list_eq);

    hash_set_iter_init(&iter, states);
    while (hash_set_iter_next(&iter, &val)) {
        sl = (StateList*) val;
        hash_set_add_all(ret->state_sets, eps_closure(tt, sl, alive_states)->state_sets);
    }

    return ret;
}

static void print_sl_hash_set(HashSet *hs)
{
    HashSetIter iter;
    gpointer val;

    hash_set_iter_init(&iter, hs);
    while (hash_set_iter_next(&iter, &val)) {
        print_state_list((StateList*) val);
    }
}

static gboolean is_final_configuration(HashSet *states, StateList *final_states)
{
    HashSetIter iter;
    gpointer val;

    hash_set_iter_init(&iter, states);
    while (hash_set_iter_next(&iter, &val)) {
        if (state_list_equals((StateList*) val, final_states))
            return TRUE;
    }
    return FALSE;
}

Prog* convert_to_prog(TransitionTable *tt_from, TransitionTable* tt_to,
                      StateList *is, StateList *fs)
{
    GHashTable *reached_states, *moves;
    GHashTableIter iter;
    GQueue *to_visit;
    GSList *to_states;
    gpointer key, value, lookup, hs_val;
    HashSetIter hs_iter;
    CollectionStateList *reachable, *curr_state, *next_state;
    HashSet *alive_states, *hs, *final_states;
    TransitionLabel *tl;
    int curr_state_id, next_state_id, index;

    alive_states = get_alive_states(tt_from, tt_to, is, fs);

#if DEBUG
    printf("ALIVE STATES:\n");
    print_sl_hash_set(alive_states);
#endif

    reached_states = g_hash_table_new(collec_state_list_hash, collec_state_list_eq);
    to_visit = g_queue_new();
    reachable = eps_closure(tt_from, is, alive_states);
    if (reachable != nil) {
        g_hash_table_insert(reached_states, reachable, 0);
        g_queue_push_head(to_visit, reachable);
    }

#if DEBUG
    printf("REACHABLE FROM INIT:\n");
    print_sl_hash_set(reachable->state_sets);
#endif

    moves = g_hash_table_new(g_direct_hash, g_direct_equal);

    while (!g_queue_is_empty(to_visit)) {
        curr_state = g_queue_pop_head(to_visit);
        curr_state_id = GPOINTER_TO_INT(g_hash_table_lookup(reached_states, curr_state));

#if DEBUG
        printf("curr_state_id = %d\n", curr_state_id);
#endif
        GHashTable *temp_ht;
        temp_ht = g_hash_table_new(transition_label_hash, transition_label_eq);

        hash_set_iter_init(&hs_iter, curr_state->state_sets);
        while (hash_set_iter_next(&hs_iter, &hs_val)) {
            value = g_hash_table_lookup(tt_from->ht, hs_val);
            if (value == nil)
                continue;

            g_hash_table_iter_init(&iter, value);
            while (g_hash_table_iter_next(&iter, &key, &value)) {
                tl = (TransitionLabel*) key;

                if (tl->label == Epsilon) continue;

                to_states = (GSList*) value;
                while (to_states != nil) {
                    if (hash_set_contains(alive_states, to_states->data)) {
#if DEBUG
                        printf("Transition: %c -> state: ", tl->info);
                        print_state_list(to_states->data);
#endif
                        add_tl_transition(temp_ht, tl, to_states->data);
                    }
                    to_states = to_states->next;
                }
            }
        }

        g_hash_table_iter_init(&iter, temp_ht);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            tl = (TransitionLabel*) key;
            assert(tl->label == Input);

            hs = (HashSet*) value;
            next_state = collec_sl_eps_closure(tt_from, hs, alive_states);
#if DEBUG
            printf("NEXT STATE [%c]:\n", tl->info);
            print_sl_hash_set(next_state->state_sets);
#endif

            lookup = g_hash_table_lookup(reached_states, next_state);
            if (lookup == nil) {
                index = g_hash_table_size(reached_states);
                g_hash_table_insert(reached_states, next_state, GINT_TO_POINTER(index));
                g_queue_push_head(to_visit, next_state);
                next_state_id = index;
            } else {
                next_state_id = GPOINTER_TO_INT(lookup);
            }

#if DEBUG
            printf("size = %d, curr_state_id = %d\n", g_hash_table_size(moves), curr_state_id);
#endif
            value = g_hash_table_lookup(moves, GINT_TO_POINTER(curr_state_id));
            if (value == nil) {
#if DEBUG
                printf("Adding new entry to moves %d -> %d [%c]\n",
                       curr_state_id, next_state_id, tl->info);
#endif
                hs = hash_set_new(state_and_tl_hash, state_and_tl_eq);
                hash_set_add(hs, make_state_and_tl(next_state_id, tl));
                g_hash_table_insert(moves, GINT_TO_POINTER(curr_state_id), hs);
            } else {
#if DEBUG
                printf("Adding to existing entry %d -> %d [%c]\n",
                       curr_state_id, next_state_id, tl->info);
#endif
                hs = (HashSet*) value;
                hash_set_add(hs, make_state_and_tl(next_state_id, tl));
                g_hash_table_replace(moves, GINT_TO_POINTER(curr_state_id), hs);
            }
        }
    }

    final_states = hash_set_new(g_direct_hash, g_direct_equal);
    g_hash_table_iter_init(&iter, reached_states);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        hs = ((CollectionStateList*) key)->state_sets;
        if (is_final_configuration(hs, fs)) {
            hash_set_add(final_states, value);
        }
    }

    return build_prog(moves, final_states);
}

int path_exists(int from, int to, Prog *prog,
                TransitionLabel *alph_sym, int final_state)
{
    Inst *inst;
    int to1, to2;

    inst = &prog->start[from];
    if (inst == nil)
        return 0;

    switch (alph_sym->label) {
    default:
        fatal("bad label in path_exists");

    case Epsilon:
        if (from == prog->len - 1 && from == final_state + 1) return 0;

        if (inst->opcode == Jmp) {
            return (inst->x - prog->start) == to;
        } else if (inst->opcode == Split) {
            to1 = (inst->x - prog->start);
            to2 = (inst->y - prog->start);
            return to1 == to || to2 == to;
        }
        return 0;
    case SubInfo:
        fatal("TODO SubInfo");
    case Input:
        if (from == to && from == final_state) {
            return 1;
        } else {
            return to == from + 1 && inst->opcode == Char && inst->c == alph_sym->info;
        }
//    case Sigma:
//        return from == to && from == final_state;
    }

    return 0;
}


/* --- creation functions -------------------------------------------------- */

TransitionTable* transition_table_new()
{
   TransitionTable *tt;

   tt = umal(sizeof(TransitionTable));
   tt->ht = g_hash_table_new(state_list_hash, state_list_eq);

   return tt;
}

TransitionLabel* make_epsilon_tl()
{
    TransitionLabel* tl;

    tl = umal(sizeof(TransitionLabel));
    tl->label = Epsilon;
    tl->info = -1;

    return tl;
}

TransitionLabel* make_char_tl(int ch)
{

    TransitionLabel* tl;

    tl = umal(sizeof(TransitionLabel));
    tl->label = Input;
    tl->info = ch;

    return tl;
}

TransitionLabel* make_transition_label(int label, int info)
{
    TransitionLabel *tl;

    tl = umal(sizeof(TransitionLabel));
    tl->label = label;
    tl->info = info;

    return tl;
}

StateList* create_state_list(int len)
{
    StateList *sl;

    sl = umal(sizeof(StateList));
    sl->states = umal(len * sizeof(int));
    sl->len = len;

    return sl;
}

StateAndTransitionLabel* make_state_and_tl(int state, TransitionLabel *tl)
{
    StateAndTransitionLabel *s;

    s = umal(sizeof(StateAndTransitionLabel));
    s->state = state;
    s->tl = tl;

    return s;
}

/* --- utility functions -------------------------------------------------- */

static char*
merge_state_list(StateList *sl)
{
    char *str;
    int i, index;

    str = mal(1024 * sizeof(char));
    index = 0;
    for (i = 0; i < sl->len; i++) {
        index += sprintf(&str[index], "%d", sl->states[i]);
    }

    return str;
}

int state_list_equals(StateList *f, StateList *t)
{
    int i;

    assert(f->len == t->len);

    for (i = 0; i < f->len; i++) {
        if (f->states[i] != t->states[i]) return 0;
    }

    return 1;
}

void add_to_alphabet(Alphabet **alph, int label, int info)
{
    if (*alph == nil) {
        *alph = umal(sizeof(Alphabet));
        (*alph)->ht = g_hash_table_new(transition_label_hash, transition_label_eq);
    }

    g_hash_table_add((*alph)->ht, (gpointer) make_transition_label(label, info));
}

/* --- printing functions ------------------------------------------------------ */

void print_dot(TransitionTable *tt,
               StateList* initial_states,
               StateList *final_states)
{
    GHashTableIter iter1, iter2;
    gpointer key1, key2, value1, value2;
    GSList *to_states;
    GList* key;
    StateList *sl;
    TransitionLabel *tl;
    char *state, *to_state, *initial;

    printf("digraph nfa {\n");
    printf("rankdir=LR;\n");

    key = g_hash_table_get_keys(tt->ht);
    while (key != nil) {
        sl = (StateList*) key->data;
        state = merge_state_list(sl);
        printf("%s", state);
        printf("[label=%s", state);

        if (state_list_equals(final_states, sl)) {
            printf(",peripheries=2");
        }

        printf("]\n");

        if (state_list_equals(initial_states, sl)) {
            printf("XX%s[color=white, label=\"\"]\n", state);
        }

        key = key->next;
    }

    initial = merge_state_list(initial_states);
    printf("XX%s -> %s\n", initial, initial);

    g_hash_table_iter_init(&iter1, tt->ht);
    while (g_hash_table_iter_next(&iter1, &key1, &value1)) {
        sl = (StateList*) key1;
        state = merge_state_list(sl);
        g_hash_table_iter_init(&iter2, value1);

        while (g_hash_table_iter_next(&iter2, &key2, &value2)) {
            tl = (TransitionLabel*) key2;
            to_states = (GSList*) value2;

            while (to_states != NULL) {
                to_state = merge_state_list((StateList*) to_states->data);
                printf("%s -> %s [label=\"", state, to_state);

                switch (tl->label) {
                default:
                    fatal("bad label in print_dot");
                case Epsilon:
                    printf("&#949;");
                    break;
                case SubInfo:
                    printf("%d", tl->info);
                    break;
                case Input:
                    printf("%c", tl->info);
                    break;
//                case Sigma:
//                    printf("&#931;");
//                    break;
                }
                printf("\"]\n");

                to_states = g_slist_next(to_states);
            }
        }

    }

    printf("}\n");
}

void print_alphabet(Alphabet *alph)
{
    GList* key;
    TransitionLabel *tl;

    key = g_hash_table_get_keys(alph->ht);

    printf("[");
    while (key != NULL) {
        tl = (TransitionLabel*) key->data;
        switch (tl->label) {
        default:
            fatal("bad label in print_alphabet");
        case Epsilon:
            printf("ε");
            break;
        case SubInfo:
            printf("SUB(%d)", tl->info);
            break;
        case Input:
            printf("%c", tl->info);
            break;
            //        case Sigma:
            //            printf("DOT");
            //            break;
        }

        if (key->next != NULL) {
            printf(", ");
        }

        key = key->next;
    }
    printf("]\n");
}

void print_state_list(StateList *sl)
{
    int i;

    printf("[");
    for (i = 0; i < sl->len; i++) {
        printf("%d", sl->states[i]);
        if (i != sl->len - 1) printf(", ");
    }
    printf("]\n");
}

/* --- hash and equality functions ----------------------------------------------- */

#define RSHIFT 5
#define LSHIFT (sizeof(unsigned int) * 8 - RSHIFT)
#define MASK   ((1 << RSHIFT) - 1)

guint state_list_hash(gconstpointer key)
{
    guint hash;
    StateList *sl;
    int i;

    sl = (StateList*) key;
    hash = 0;
    for (i = 0; i < sl->len; i++) {
        hash += sl->states[i];
        hash = (hash << RSHIFT) | ((hash >> LSHIFT) & MASK);
    }

    return hash;
}

gboolean state_list_eq(gconstpointer a, gconstpointer b)
{
    return state_list_equals((StateList*) a, (StateList*) b);
}

guint collec_state_list_hash(gconstpointer key)
{
    guint hash;
    gpointer val;
    HashSet *set;
    HashSetIter iter;
    StateList *sl;
    int i;

    set = ((CollectionStateList*) key)->state_sets;
    hash = 0;

    hash_set_iter_init(&iter, set);
    while (hash_set_iter_next(&iter, &val)) {
        sl = (StateList*) val;
        for (i = 0; i < sl->len; i++) {
            hash += sl->states[i];
            hash = (hash << RSHIFT) | ((hash >> LSHIFT) & MASK);
        }
    }

    return hash;
}

gboolean collec_state_list_eq(gconstpointer a, gconstpointer b)
{
    HashSet *set1, *set2;
    HashSetIter iter;
    gpointer val;
    guint size1, size2;

    set1 = ((CollectionStateList*) a)->state_sets;
    size1 = hash_set_size(set1);

    set2 = ((CollectionStateList*) b)->state_sets;
    size2 = hash_set_size(set2);

    if (size1 != size2) return FALSE;

    hash_set_iter_init(&iter,  set1);
    while (hash_set_iter_next(&iter, &val)) {
        if (!hash_set_contains(set2, val)) {
            return FALSE;
        }
    }

    return TRUE;
}

guint transition_label_hash(gconstpointer key)
{
    guint hash;
    TransitionLabel *tl;

    tl = (TransitionLabel*) key;

    switch (tl->label) {
    default:
        fatal("bad label in transition_label_hash");
    case Input:
        hash = tl->info;
        break;
    case Epsilon:
        hash = 0;
        break;
    case SubInfo:
        hash = tl->info;
        break;
    }

    return (hash << RSHIFT) | ((hash >> LSHIFT) & MASK);
}

gboolean transition_label_eq(gconstpointer a, gconstpointer b)
{
    TransitionLabel *tl1, *tl2;

    tl1 = (TransitionLabel*) a;
    tl2 = (TransitionLabel*) b;

    if (tl1->label != tl2->label)
        return FALSE;

    if (tl1->label == Epsilon)
        return TRUE;

    return tl1->info == tl2->info;
}

guint state_and_tl_hash(gconstpointer key)
{
    StateAndTransitionLabel *k;
    guint hash;

    k = (StateAndTransitionLabel*) key;

    hash = k->state;
    return (hash << RSHIFT) | ((hash >> LSHIFT) & MASK);
}

gboolean state_and_tl_eq(gconstpointer a, gconstpointer b)
{
    StateAndTransitionLabel *l, *r;

    l = (StateAndTransitionLabel*) a;
    r = (StateAndTransitionLabel*) b;

    if (l->state != r->state) return FALSE;

    return transition_label_eq(l->tl, r->tl);
}
