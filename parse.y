// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

%{
#include "regexp.h"

static int yylex(void);
static void yyerror(char*);
static Regexp *parsed_regexp;
static int nparen;
static int nlookaheads;

%}

%union {
    Regexp *re;
    int c;
    int nparen;
    int nlookaheads;
}

%token	<c> CHAR EOL
%type	<re>	alt concat repeat single line
%type	<nparen> count
%type   <nlookaheads> count_look

%%

line: alt EOL
{
    parsed_regexp = $1;
    return 1;
}

alt:
concat
|	alt '|' concat
{
    $$ = reg(Alt, $1, $3);
}
;

concat:
repeat
|	concat repeat
{
    $$ = reg(Cat, $1, $2);
}
;

repeat:
single
|	single '*'
{
    $$ = reg(Star, $1, nil);
}
|	single '*' '?'
{
    $$ = reg(Star, $1, nil);
    $$->n = 1;
}
|	single '+'
{
    $$ = reg(Plus, $1, nil);
}
|	single '+' '?'
{
    $$ = reg(Plus, $1, nil);
    $$->n = 1;
}
|	single '?'
{
    $$ = reg(Quest, $1, nil);
}
|	single '?' '?'
{
    $$ = reg(Quest, $1, nil);
    $$->n = 1;
}
;

count:
{
$$ = ++nparen;
}
;

count_look:
{
$$ = ++nlookaheads;
}
;

single:
'(' count alt ')'
{
    $$ = reg(Paren, $3, nil);
    $$->n = $2;
}
|   '(' '?' '=' count_look alt ')'
{
    $$ = reg(Look, $5, nil);
}
|	'(' '?' ':' alt ')'
{
    $$ = $4;
}
|	CHAR
{
    $$ = reg(Lit, nil, nil);
    $$->ch = $1;
}
|	'.'
{
$$ = reg(Dot, nil, nil);
}
;

%%

static char *input;
static Regexp *parsed_regexp;
static int nparen;
static int nlookaheads;
int gen;

static int
yylex(void)
{
    int c;

    if (input == NULL || *input == 0)
        return EOL;
    c = *input++;
    if (strchr("|*+?=():.", c))
        return c;
    yylval.c = c;
    return CHAR;
}

void
fatal(char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    fprintf(stderr, "fatal error: ");
    vfprintf(stderr, fmt, arg);
    fprintf(stderr, "\n");
    va_end(arg);
    exit(2);
}

static void
yyerror(char *s)
{
    fatal("%s", s);
}


RegexpWithLook*
parse(char *s)
{
    Regexp *r, *dotstar;

    input = s;
    parsed_regexp = nil;
    nparen = 0;
    if (yyparse() != 1)
        yyerror("did not parse");
    if (parsed_regexp == nil)
        yyerror("parser nil");

//    r = reg(Paren, parsed_regexp, nil);	// $0 parens
//    dotstar = reg(Star, reg(Dot, nil, nil), nil);
//    dotstar->n = 1;	// non-greedy
//    return regWithLook(reg(Cat, dotstar, r), nlookaheads);

    return regWithLook(reg(Paren, parsed_regexp, nil), nlookaheads);
}

void*
mal(int n)
{
    return imal(n, 0);
}

void*
imal(int n, int val)
{
    void *v;

    v = malloc(n);
    if (v == nil)
        fatal("out of memory");
    memset(v, val, n);
    return v;
}

void*
umal(int n)
{
    void *v;

    v = malloc(n);
    if (v == nil)
        fatal("out of memory");

    return v;
}

Regexp*
reg(int type, Regexp *left, Regexp *right)
{
    Regexp *r;

    r = mal(sizeof *r);
    r->type = type;
    r->left = left;
    r->right = right;
    return r;
}

RegexpWithLook*
regWithLook(Regexp *reg, int k)
{
    RegexpWithLook *rwl;

    rwl = mal(sizeof *rwl);
    rwl->regexp = reg;
    rwl->k = k;
    return rwl;
}

void
printre(Regexp *r)
{
    switch (r->type) {
    default:
        printf("???");
        break;

    case Alt:
        printf("Alt(");
        printre(r->left);
        printf(", ");
        printre(r->right);
        printf(")");
        break;

    case Cat:
        printf("Cat(");
        printre(r->left);
        printf(", ");
        printre(r->right);
        printf(")");
        break;

    case Lit:
        printf("Lit(%c)", r->ch);
        break;

    case Dot:
        printf("Dot");
        break;

    case Paren:
        printf("Paren(%d, ", r->n);
        printre(r->left);
        printf(")");
        break;

    case Look:
        printf("Look(%d, ", r->n);
        printre(r->left);
        printf(")");
        break;

    case Star:
        if (r->n)
            printf("Ng");
        printf("Star(");
        printre(r->left);
        printf(")");
        break;

    case Plus:
        if (r->n)
            printf("Ng");
        printf("Plus(");
        printre(r->left);
        printf(")");
        break;

    case Quest:
        if (r->n)
            printf("Ng");
        printf("Quest(");
        printre(r->left);
        printf(")");
        break;
    }
}
