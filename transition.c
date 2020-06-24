#include <assert.h>
#include "transition.h"

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

static CollectionStateList*
eps_closure(TransitionTable *transition_table, StateList *curr_state, size_t *length)
{
    CollectionStateList *reachable;
    GHashTable *reached;

    reachable = nil;

    return reachable;
}

Prog* convert_to_prog(TransitionTable *transition_table, StateList *is, StateList *fs)
{
    GHashTable *reached_states;
    CollectionStateList *to_visit[1024];
    Prog *p;
    CollectionStateList *reachable, *curr_state, *next_state;
    size_t length;
    int curr_state_id, tv;

    reached_states = g_hash_table_new(collec_state_list_hash, collec_state_list_eq);

    tv = 0;
    reachable = eps_closure(transition_table, is, &length);
    if (reachable != nil) {
        g_hash_table_insert(reached_states, reachable, 0);
        to_visit[tv++] = reachable;
    }

    while (tv > 0) {
        curr_state = to_visit[--tv];
        curr_state_id = GPOINTER_TO_INT(g_hash_table_lookup(reached_states, curr_state));
        printf("curr_state_id = %d\n", curr_state_id);
    }

    p = nil;

    return p;
}

int path_exists(int from, int to, Prog *prog, Alphabet *alph, int final_state)
{
    Inst *inst;
    int to1, to2;

    inst = &prog->start[from];
    if (inst == nil)
        return 0;

    switch (alph->label) {
    default:
        fatal("bad label in transition_exists");

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
            return to == from + 1 && inst->opcode == Char && inst->c == alph->info;
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

Alphabet* make_alphabet_pair(int label, int info)
{
   Alphabet *a;

   a = umal(sizeof(Alphabet));
   a->label = label;
   a->info = info;
   a->next = nil;

   return a;
}

StateList* create_state_list(int len)
{
    StateList *sl;

    sl = umal(sizeof(StateList));
    sl->states = umal(len * sizeof(int));
    sl->len = len;

    return sl;
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
    Alphabet *t, *tt;

    if (*alph == nil) {
        *alph = make_alphabet_pair(label, info);
    } else {
        t = *alph;
        tt = t;
        while (t != nil) {
            /* is the item already in the list? */
            if (t->label == label && t->info == info) {
                return;
            }

            tt = t;
            t = t->next;
        }

        tt->next = make_alphabet_pair(label, info);
    }
}

/* --- printing functions -------------------------------------------------- */

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

void print_alphabet(Alphabet *a)
{
    Alphabet *t;
    int eps;

    t = a;
    eps = 0;
    printf("[");
    while (t != nil) {
        switch (t->label) {
        default:
            fatal("bad label in print_pair_list");
        case Epsilon:
            if (!eps) {
                printf("ε");
                eps = 1;
            }
            break;
        case SubInfo:
            printf("SUB(%d)", t->info);
            break;
        case Input:
            printf("%c", t->info);
            break;
//        case Sigma:
//            printf("DOT");
//            break;
        }
        if (t->next != nil) {
            if (!(t->next->label == Epsilon && eps)) {
                printf(", ");
            }
        }
        t = t->next;
    }
    printf("]\n");
}

/* --- hash functions -------------------------------------------------- */

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
    CollectionStateList *csl;
    int i, j;

    csl = (CollectionStateList*) key;
    hash = 0;
    for (i = 0; i < csl->len; i++) {
        for (j = 0; j < csl->state_lists[i]->len; j++) {
            hash += csl->state_lists[i]->states[j];
            hash = (hash << RSHIFT) | ((hash >> LSHIFT) & MASK);
        }
    }

    return hash;
}

gboolean collec_state_list_eq(gconstpointer a, gconstpointer b)
{
    CollectionStateList *csl1, *csl2;
    int i;

    csl1 = (CollectionStateList*) a;
    csl2 = (CollectionStateList*) b;

    if (csl1->len != csl2->len) return FALSE;

    for (i = 0; i < csl1->len; i++) {
        if (!state_list_equals(csl1->state_lists[i], csl2->state_lists[i])) return FALSE;
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

