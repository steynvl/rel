#include <assert.h>
#include "transition.h"

int state_list_eq(StateList *f, StateList *t)
{
    int i;

    assert(f->len == t->len);

    for (i = 0; i < f->len; i++) {
        if (f->states[i] != t->states[i]) return 0;
    }

    return 1;
}

static int contains_state_list(StateList **list, int len, StateList *target)
{
    int i;

    for (i = 0; i < len; i++) {
        if (state_list_eq(list[i], target)) return 1;
    }

    return 0;
}

void remove_dead_states(Transition **transitions, StateList *initial_states)
{
    TransitionFrag *tf;
    Transition *c, *s, *initial;
    int added, tv;

    /* array of visited states */
    StateList **visited = mal(1024 * sizeof(StateList));
    Transition **tovisit = mal(1024 * sizeof(Transition));
    added = 0;
    tv = 0;

    assert((*transitions) != nil);

    initial = *transitions;
    while (initial != nil && !state_list_eq(initial->from, initial_states)) {
        initial = initial->next;
    }

    /* TODO return empty transitions [not sure what to do here] */
    if (initial == nil) return;

    tovisit[tv++] = initial;

    while (tv > 0) {
        c = tovisit[--tv];
        if (!contains_state_list(visited, added, c->from)) {
            visited[added++] = c->from;
        }

        tf = c->to;
        while (tf != nil) {
            if (!contains_state_list(visited, added, tf->to_states)) {
                visited[added++] = tf->to_states;

                s = *transitions;
                while (s != nil) {
                    if (state_list_eq(s->from, tf->to_states)) {
                        tovisit[tv++] = s;
                        break;
                    }
                    s = s->next;
                }
            }

            tf = tf->next;
        }
    }

    c = *transitions;
    s = c;
    while (c != nil) {
        if (!contains_state_list(visited, added, c->from)) {
            if (s == c) {
                *transitions = (*transitions)->next;
                c = *transitions;
                s = c;
            } else {
                s->next = c->next;
                c = c->next;
            }
            continue;
        }

        s = c;
        c = c->next;
    }

}

void add_transition(Transition **transitions, StateList *from,
                    StateList *to, Pair *info)
{
    Transition *to_add, *last;
    TransitionFrag *tf, *ltf;
    int key_exists;

    key_exists = 0;
    to_add = *transitions;
    last = nil;
    while (to_add != nil) {
        if (state_list_eq(to_add->from, from)) {
            key_exists = 1;
            break;
        }

        last = to_add;
        to_add = to_add->next;
    }

    if (!key_exists) {
        to_add = mal(sizeof(Transition));
        to_add->from = from;

        to_add->to = mal(sizeof(TransitionFrag));
        to_add->to->to_states = to;
        to_add->to->pairs = info;
        to_add->to->next = nil;

        if (last == nil) {
            *transitions = to_add;
        } else {
            last->next = to_add;
        }

        return;
    }

    tf = to_add->to;
    ltf = tf;
    while (tf != nil) {
        ltf = tf;
        tf = tf->next;
    }

    ltf->next = mal(sizeof(TransitionFrag));
    ltf->next->to_states = to;
    ltf->next->pairs = info;
    ltf->next->next = nil;
}

int transition_exists(int from, int to, Prog *prog, Pair *p, int final_state)
{
    Inst *inst;
    int to1, to2;

    inst = &prog->start[from];
    if (inst == nil)
        return 0;

    switch (p->label) {
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
            return to == from + 1 && inst->opcode == Char && inst->c == p->info;
        }
//    case Sigma:
//        return from == to && from == final_state;
    }

    return 0;
}

#if 0
Pair* get_transition(int from, int to, Prog *prog)
{
    Pair *p;
    Inst *inst;
    int to1, to2;

    inst = &prog->start[from];
    if (inst == nil) {
        return nil;
    }

    p = mal(sizeof(Pair));
    p->info = -1;

    /* final state of main and lookahead branches have Sigma self-loop */
    if (from == to && from == prog->len - 1) {
        p->label = Sigma;
        return p;
    }

    switch (inst->opcode) {
    default:
        fatal("bad opcode in get_transition");
    case Char:
        p->info = inst->c;
        p->label = Input;
        break;
    case Match:
        free(p);
        return nil;
    case Jmp:
        to1  = (inst->x - prog->start);
        if (to != to1) {
            return nil;
        }
        p->label = Epsilon;
        return p;
    case Split:
        to1 = (inst->x - prog->start);
        to2 = (inst->y - prog->start);

        if (to != to1 && to != to2) {
            free(p);
            return nil;
        }

        p->label = Epsilon;
        return p;
    case Any:
        p->label = Sigma;
        break;
    case Save:
        p->label = SubInfo;
        p->info = inst->n;
        break;
    }

    to1 = from + 1;
    assert(to1 >= 0 && to1 < prog->len);

    if (to != to1) {
        free(p);
        return nil;
    }

    return p;
}
#endif

void add_to_pair_list(Pair **p, int label, int info)
{
    Pair *t, *tt;

    if (*p == nil) {
        *p = mal(sizeof(Pair));
        (*p)->label = label;
        (*p)->info = info;
        (*p)->next = nil;
    } else {
        t = *p;
        tt = t;
        while (t != nil) {
            /* is the item already in the list? */
            if (t->label == label && t->info == info) {
                return;
            }

            tt = t;
            t = t->next;
        }

        t = mal(sizeof(Pair));
        t->label = label;
        t->info = info;
        t->next = nil;

        tt->next = t;
    }
}

StateList* create_state_list(int len)
{
    StateList *sl;

    sl = mal(sizeof(StateList));
    sl->states = mal(len * sizeof(int));
    sl->len = len;

    return sl;
}

void print_pair_list(Pair* p)
{
    Pair *t;
    int eps;

    t = p;
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

void print_transition_table(Transition *t)
{
    Transition *curr;

    printf("TRANSITIONS:\n");
    curr = t;
    while (curr != nil) {
        print_transition(curr->from, curr->to);
        curr = curr->next;
    }
}

static void
print_state_list(StateList *sl)
{
    int i;

    printf("(");
    for (i = 0; i < sl->len; i++) {
        printf("%d", sl->states[i]);
        if (i != sl->len - 1) {
            printf(", ");
        }
    }
    printf(")");
}

void print_transition(StateList *from, TransitionFrag *to)
{
    TransitionFrag *tf;

    tf = to;

    while (tf != nil) {
        print_state_list(from);
        printf(" -> ");
        print_state_list(tf->to_states);
        printf(" ");
        print_pair_list(tf->pairs);
        printf("\n");

        tf = tf->next;
    }
}

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

void print_dot(Transition *t, StateList* initial_states, StateList *final_states)
{
    Transition *curr;
    TransitionFrag *tf;
    Pair *p;
    char *state, *to_state, *initial;

    printf("digraph nfa {\n");
    printf("rankdir=LR;\n");

    curr = t;
    while (curr != nil) {
        state = merge_state_list(curr->from);
        printf("%s", state);
        printf("[label=%s", state);

        if (state_list_eq(final_states, curr->from)) {
            printf(",peripheries=2");
        }

        printf("]\n");

        if (state_list_eq(initial_states, curr->from)) {
            printf("XX%s[color=white, label=\"\"]\n", state);
        }

        curr = curr->next;
    }

    initial = merge_state_list(initial_states);
    printf("XX%s -> %s\n", initial, initial);

    curr = t;
    while (curr != nil) {
        tf = curr->to;

        while (tf != nil) {
            state = merge_state_list(curr->from);
            to_state = merge_state_list(tf->to_states);

            p = tf->pairs;
            while (p != nil) {
                printf("%s -> %s [label=\"", state, to_state);

                switch (p->label) {
                default:
                    fatal("bad label in print_dot");
                case Epsilon:
                    printf("&#949;");
                    break;
                case SubInfo:
                    printf("%d", p->info);
                    break;
                case Input:
                    printf("%c", p->info);
                    break;
//                case Siigma:
//                    printf("&#931;");
//                    break;
                }
                printf("\"]\n");

                p = p->next;
            }

            tf = tf->next;
        }


        curr = curr->next;
    }


    printf("}\n");
}
