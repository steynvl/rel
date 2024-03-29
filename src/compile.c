// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "utils.h"

static Inst *pc;

/* current index [0 = main] [1 = 1st lookahead branch] ... */
static int ci;

/* seen lookaheads, used in count function */
static int sl;

/* current branch, used in emit to keep track of nested lookaheads */
static int cb;

/* states which were originally the "and" state starting lookahead i */
static AndState* and_states;

/* alphabet of the regex, populated in emit */
static Alphabet* alphabet;

/* map state to capture sub */
static GHashTable *save_states;

/* --- function prototypes -------------------------------------------------- */
static void count(Regexp*, int*, int);
static Prog* construct_product(ProgWithLook*);
static void product_transitions(TransitionTable*, TransitionTable*, int**,
                                int, int*, ProgWithLook*, FinalStates*);
static void emit(Regexp*, ProgWithLook*);
static void populate_states(ProgWithLook*, int**, int);

/* implemented in buildprog.c */
void make_lazy_match_anything(Inst**);

Prog*
compile(RegexpWithLook *rwl)
{
    ProgWithLook *pwl;

    Regexp *r;
    int *n;
    int i, len, num_progs;

    r = rwl->regexp;
    num_progs = rwl->k + 1;

#if DEBUG
    printf("num_progs = %d\n", num_progs);
#endif

    n = umal(num_progs * sizeof *n);
    for (i = 0; i < num_progs; i++) n[i] = 1;

    sl = 0;
    count(r, n, 0);

    if (num_progs == 1)
        n[0] += 3;

#if DEBUG
    for (i = 0; i < num_progs; i++) {
        printf("n[%d] = %d\n", i, n[i]);
    }
#endif

    pwl = create_prog_with_look(num_progs, n);

    ci = 0, cb = 0;
    and_states = umal(rwl->k * sizeof(AndState));
    pc = pwl[ci].prog->start;

    alphabet = nil;
    add_to_alphabet(&alphabet, Epsilon, -1);
    save_states = g_hash_table_new(g_direct_hash, g_direct_equal);

    /* if the regex has no lookaheads, then we add
       prepend (.*?) to the program to skip the prefix
       in the text before the match starts. If the regex
       contains lookaheads then this step will be performed
       in buildprog. */
    if (num_progs == 1)
        make_lazy_match_anything(&pc);

    emit(r, pwl);

#if DEBUG
    printf("ALPHABET!\n");
    print_alphabet(alphabet);

    for (i = 0; i < rwl->k; i++) {
        printf("and_states[%d] = (state = %d, branch = %d)\n", i, and_states[i].state, and_states[i].branch);
    }
#endif

    for (i = 0; i < num_progs; i++) {
        len = pwl[i].prog->len;

        pwl[i].prog->start[len].opcode = Match;
        pwl[i].prog->len += 1;

#if DEBUG
        printf("Program %d\n", i);
        printprog(pwl[i].prog);
#endif
    }

    return num_progs == 1 ? pwl[0].prog : construct_product(pwl);
}

static Prog*
construct_product(ProgWithLook *pwl)
{
    AndState as;
    TransitionTable *tt_from, *tt_to;
    /* final and initial states of the product automaton */
    StateList *fs1, *fs2, *is;
    FinalStates *fs;
    int *offsets;
    int **states;
    int i, num_states, cum;

    offsets = umal(pwl->len * sizeof(int));
    cum = 0, num_states = 1;
    for (i = 0; i < pwl->len; i++) {
        if (i > 0) pwl[i].prog->len++;

        num_states *= pwl[i].prog->len;

        offsets[i] = cum;
        cum += pwl[i].prog->len;

        if (i < pwl->len - 1) {
            as = and_states[i];
            as.state += offsets[as.branch];
        }
    }

    states = create_2d_arr(num_states, pwl->len);
    populate_states(pwl, states, num_states);

#if DEBUG
    print_states(states, num_states, pwl->len, offsets);
#endif

    fs1 = create_state_list(pwl->len);
    fs2 = create_state_list(pwl->len);
    is = create_state_list(pwl->len);
    for (i = 0; i < pwl->len; i++) {
        fs1->states[i] = i > 0 ? pwl[i].prog->len - 2 : pwl[i].prog->len - 1;
        fs1->states[i] += offsets[i];
        fs2->states[i] = i > 0 ? pwl[i].prog->len - 1 : pwl[i].prog->len - 1;
        fs2->states[i] += offsets[i];

        is->states[i] = i > 0 ? offsets[i] + pwl[i].prog->len - 1 : offsets[i];
    }
    fs = create_final_states();
    hash_set_add(fs->states, fs1);
    hash_set_add(fs->states, fs2);

    tt_from = transition_table_new();
    tt_to = transition_table_new();
    product_transitions(tt_from, tt_to, states, num_states, offsets, pwl, fs);

    print_state_list(is);
    // TODO print_final_states(fs)

#if DEBUG
    print_dot(tt_from, is, fs);
#endif

    return convert_to_prog(tt_from, tt_to, is, fs, save_states);
}

static int
contains_and(int original_and, int *states, int *offsets, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        if (original_and == states[i] + offsets[i])
            return 1;
    }

    return 0;
}

static void
product_transitions(TransitionTable *tt_from, TransitionTable *tt_to,
                    int **states, int num_states, int *offsets,
                    ProgWithLook *pwl, FinalStates *final_states)
{
    TransitionLabel *tl, *alph_sym;
    GHashTableIter iter;
    gpointer key, val;
    gboolean satisfied, is_eq;
    StateList *states_from, *states_to;
    int r1, r2, i, q, q_prime, original_and, fs;
    int is_fresh1, is_fresh2, does_contain_and;
    int *t1, *t2;

#if DEBUG
    for (i = 0; i < pwl->len; i++)
        printf("offsets[%d] = %d\n", i, offsets[i]);
#endif

    for (r1 = 0; r1 < num_states; r1++) {
        for (r2 = 0; r2 < num_states; r2++) {
            t1 = states[r1];
            t2 = states[r2];

            g_hash_table_iter_init(&iter, alphabet->ht);
            while (g_hash_table_iter_next(&iter, &key, &val)) {
                alph_sym = (TransitionLabel*) key;
                satisfied = TRUE;

                for (i = pwl->len - 1; i >= 0; i--) {
                    q = t1[i] + offsets[i];
                    q_prime = t2[i] + offsets[i];
                    is_fresh1 = t1[i] == pwl[i].prog->len - 1;
                    is_fresh2 = t2[i] == pwl[i].prog->len - 1;

                    /* submatch save state */
                    if (i == 0 && q_prime == q + 1
                            && alph_sym->label == Epsilon
                            && g_hash_table_contains(save_states, GINT_TO_POINTER(q))) {
                        continue;
                    }

                    /* α = ε and q_i' = q_i */
                    if (!is_fresh1 && alph_sym->label == Epsilon && q == q_prime) {
                        continue;
                    }

                    fs = i > 0 ? pwl[i].prog->len - 2 : pwl[i].prog->len - 1;
                    /* (q_i, α, q_i') ∈ δ_i */
                    if (path_exists(t1[i], t2[i], pwl[i].prog, alph_sym, fs)) {
                        continue;
                    }

                    if (i == 0) {
                        satisfied = FALSE;
                        break;
                    }

                    original_and = and_states[i - 1].state;
                    does_contain_and = contains_and(original_and, t2, offsets, pwl->len);

                    /* q_i = q_i' = ⊥ */
                    if (is_fresh1 && is_fresh2) {
                        if (does_contain_and) {
                            satisfied = FALSE;
                            break;
                        }
                        continue;
                    }

                    /* q_i = ⊥, q_i' = I_i */
                    if (is_fresh1 && q_prime == offsets[i]) {
                        if (!does_contain_and) {
                            satisfied = FALSE;
                            break;
                        }
                        continue;
                    }

                    satisfied = FALSE;
                    break;
                }

                if (satisfied) {
                    states_from = create_state_list(pwl->len);
                    states_to = create_state_list(pwl->len);
                    is_eq = TRUE;
                    for (i = 0; i < pwl->len; i++) {
                        states_from->states[i] = t1[i] + offsets[i];
                        states_to->states[i] = t2[i] + offsets[i];
                        if (states_from->states[i] != states_to->states[i])
                            is_eq = FALSE;
                    }

                    if (!(is_eq && hash_set_contains(final_states->states, states_from))) {
                        tl = make_transition_label(alph_sym->label, alph_sym->info);
                        add_sl_transition(tt_from, states_from, states_to, tl);
                        add_sl_transition(tt_to, states_to, states_from, tl);
                    }
                }

                states_from = nil;
                states_to = nil;
            }

        }
    }
}

static void
populate_states(ProgWithLook *pwl, int **states, int num_states)
{
    int **cpy;
    int i, j, k, end, width, added, rem_look;

    cpy = create_2d_arr(num_states, pwl->len);
    for (i = 0; i < pwl[0].prog->len; i++) {
        cpy[i][0] = i;
    }

    end = i;
    width = 1;

    rem_look = pwl->len - 1;
    while (rem_look > 0) {

        added = 0;
        for (i = 0; i < end; i++) {
            for (j = 0; j < pwl[pwl->len - rem_look].prog->len; j++) {
                for (k = 0; k < width; k++) {
                    states[added][k] = cpy[i][k];
                }
                states[added++][width] = j;
            }
        }

        end = added;
        rem_look--;

        for (i = 0; i < num_states; i++) {
            for (j = 0; j < pwl->len; j++) {
                cpy[i][j] = states[i][j];
            }
        }

        width++;
    }

    for (i = 0; i < num_states; i++) {
        free(cpy[i]);
    }
    free(cpy);
}

// how many instructions does r and the k lookaheads need?
static void
count(Regexp *r, int *counts, int index)
{
    switch (r->type) {
    default:
        fatal("bad count");
    case Alt:
        counts[index] += 2;
        count(r->left, counts, index);
        count(r->right, counts, index);
        return;
    case Cat:
        count(r->left, counts, index);
        count(r->right, counts, index);
        return;
    case Lit:
    case Dot:
        counts[index] += 1;
        return;
    case Paren:
        counts[index] += 2;
        count(r->left, counts, index);
        return;
    case Look:
        count(r->left, counts, ++sl);
        return;
    case Quest:
        counts[index] += 1;
        count(r->left, counts, index);
        return;
    case Star:
        counts[index] += 2;
        count(r->left, counts, index);
        return;
    case Plus:
        counts[index] += 1;
        count(r->left, counts, index);
        return;
    }
}

static void
emit(Regexp *r, ProgWithLook *pwl)
{
    Inst *p1, *p2, *t;
    int idx, n;

    switch (r->type) {
    default:
        fatal("bad emit");

    case Look:
        and_states[ci].state = pc - pwl[cb].prog->start;
        and_states[ci].branch = cb;
        p1 = pc;
        pc = pwl[++ci].prog->start;
        cb++;
        emit(r->left, pwl);
        cb--;
        pc = p1;
        break;

    case Alt:
        pc->opcode = Split;
        p1 = pc++;
        p1->x = pc;
        emit(r->left, pwl);
        pc->opcode = Jmp;
        p2 = pc++;
        p1->y = pc;
        emit(r->right, pwl);
        p2->x = pc;
        break;

    case Cat:
        emit(r->left, pwl);
        emit(r->right, pwl);
        break;

    case Lit:
        pc->opcode = Char;
        pc->c = r->ch;
        pc++;
        add_to_alphabet(&alphabet, Input, r->ch);
        break;

    case Dot:
        pc++->opcode = Any;
        /* TODO add to alphabet */
        break;

    case Paren:
        idx = pc - pwl[cb].prog->start;
        n = 2*r->n;
        assert(g_hash_table_insert(save_states, GINT_TO_POINTER(idx), GINT_TO_POINTER(n)));
        pc->opcode = Save;
        pc->n = n;
        pc++;
        emit(r->left, pwl);
        idx = pc - pwl[cb].prog->start;
        n = 2*r->n + 1;
        assert(g_hash_table_insert(save_states, GINT_TO_POINTER(idx), GINT_TO_POINTER(n)));
        pc->opcode = Save;
        pc->n = n;
        pc++;
        break;

    case Quest:
        pc->opcode = Split;
        p1 = pc++;
        p1->x = pc;
        emit(r->left, pwl);
        p1->y = pc;
        if (r->n) {	// non-greedy
            t = p1->x;
            p1->x = p1->y;
            p1->y = t;
        }
        break;

    case Star:
        pc->opcode = Split;
        p1 = pc++;
        p1->x = pc;
        emit(r->left, pwl);
        pc->opcode = Jmp;
        pc->x = p1;
        pc++;
        p1->y = pc;
        if (r->n) {	// non-greedy
            t = p1->x;
            p1->x = p1->y;
            p1->y = t;
        }
        break;

    case Plus:
        p1 = pc;
        emit(r->left, pwl);
        pc->opcode = Split;
        pc->x = p1;
        p2 = pc;
        pc++;
        p2->y = pc;
        if (r->n) {	// non-greedy
            t = p2->x;
            p2->x = p2->y;
            p2->y = t;
        }
        break;
    }
}

void
printprog(Prog *p)
{
    Inst *pc;
    int i;

    pc = p->start;

    for (i = 0; i < p->len; i++) {
        pc = &p->start[i];
        switch (pc->opcode) {
        default:
            fatal("printprog");
        case Split:
            printf("%2d. split %d, %d\n", (int)(pc-p->start), (int)(pc->x-p->start), (int)(pc->y-p->start));
            break;
        case Jmp:
            printf("%2d. jmp %d\n", (int)(pc-p->start), (int)(pc->x-p->start));
            break;
        case Char:
            printf("%2d. char %c\n", (int)(pc-p->start), pc->c);
            break;
        case Any:
            printf("%2d. any\n", (int)(pc-p->start));
            break;
        case Match:
            printf("%2d. match\n", (int)(pc-p->start));
            break;
        case Save:
            printf("%2d. save %d\n", (int)(pc-p->start), pc->n);
        }
    }
}
