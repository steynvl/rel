// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef RE1_REGEXP_H
#define RE1_REGEXP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#define nil ((void*)0)
#define nelem(x) (sizeof(x)/sizeof((x)[0]))

typedef struct Regexp Regexp;
typedef struct RegexpWithLook RegexpWithLook;

typedef struct Prog Prog;
typedef struct ProgWithLook ProgWithLook;

typedef struct Inst Inst;

typedef struct AndState AndState;

struct RegexpWithLook
{
    /* number of lookaheads in the REwLA */
    int k;

    Regexp *regexp;
};

struct AndState {
    /* branch of "and" state, 0=main, 1=lookahead 1, 2=lookahead 2... */
    int branch;

    int state;
};

struct Regexp
{
    int type;
    int n;
    int ch;
    Regexp *left;
    Regexp *right;
};

enum	/* Regexp.type */
{
    Alt = 1,
    Cat,
    Lit,
    Dot,
    Paren,
    Look,
    Quest,
    Star,
    Plus,
};

RegexpWithLook *parse(char*);
Regexp *reg(int type, Regexp *left, Regexp *right);
RegexpWithLook *regWithLook(Regexp *reg, int k);
void printre(Regexp*);
void fatal(char*, ...);
void *mal(int);
void *imal(int, int);
void *umal(int);

struct Prog
{
    Inst *start;
    int len;
};

struct ProgWithLook {
    Prog *prog;
    int len;
};

struct Inst
{
    int opcode;
    int c;
    int n;
    Inst *x;
    Inst *y;
    int gen;	// global state, oooh!
};

enum	/* Inst.opcode */
{
    Char = 1,
    Match,
    Jmp,
    Split,
    Any,
    Save
};

Prog *compile(RegexpWithLook*);
void printprog(Prog*);

extern int gen;

enum {
    MAXSUB = 20
};

typedef struct Sub Sub;

struct Sub
{
    int ref;
    int nsub;
    char *sub[MAXSUB];
};

Sub *newsub(int n);
Sub *incref(Sub*);
Sub *copy(Sub*);
Sub *update(Sub*, int, char*);
void decref(Sub*);

int pikevm(Prog*, char*, char**, int);

#endif
