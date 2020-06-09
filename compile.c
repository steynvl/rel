// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "regexp.h"

static Inst *pc;

/* current index [0 = main] [1 = 1st lookahead branch] ... */
static int ci;

static void count(Regexp*, int*, int);
static Prog* construct_product(ProgWithLook*);
static void emit(Regexp*, ProgWithLook*);

Prog*
compile(RegexpWithLook *rwl)
{
    ProgWithLook *pwl;
    Prog *p;

    Regexp *r;
    int *n;
    int i, num_progs;

    r = rwl->regexp;
    num_progs = rwl->k + 1;
    printf("num_progs = %d\n", num_progs);

    n = mal(num_progs * sizeof *n);
    for (i = 0; i < num_progs; i++) {
        n[i] = 1;
    }

    count(r, n, 0);

    for (i = 0; i < num_progs; i++) {
        printf("n[%d] = %d\n", i, n[i]);
    }

    pwl = mal(num_progs * sizeof(ProgWithLook));
    pwl->len = num_progs;
    for (i = 0; i < num_progs; i++) {
        p = mal(sizeof *p + n[i]*sizeof p->start[0]);
        p->start = (Inst*)(p+1);
        p->len = n[i] - 1;
        pwl[i].prog = p;
    }

    ci = 0;
    pc = pwl[ci].prog->start;

    emit(r, pwl);

    for (i = 0; i < num_progs; i++) {
        int len = pwl[i].prog->len;

        pwl[i].prog->start[len].opcode = Match;
        pwl[i].prog->len += 1;

        printf("PROGRAM %d\n", i);
        printprog(pwl[i].prog);
    }

    return construct_product(pwl);
}

static Prog*
construct_product(ProgWithLook *pwl)
{
    return nil;
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
        count(r->left, counts, index + 1);
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

    switch (r->type) {
    default:
        fatal("bad emit");

    case Look:
        p1 = pc;
        pc = pwl[++ci].prog->start;
        emit(r->left, pwl);
        pc = p1;
        ci--;
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
        break;

    case Dot:
        pc++->opcode = Any;
        break;

    case Paren:
        pc->opcode = Save;
        pc->n = 2*r->n;
        pc++;
        emit(r->left, pwl);
        pc->opcode = Save;
        pc->n = 2*r->n + 1;
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

