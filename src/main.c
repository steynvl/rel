// Copyright 2007-2009 Russ Cox.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "regex.h"

void
usage(void)
{
    fprintf(stderr, "usage: re regexp string...\n");
    exit(2);
}

int
main(int argc, char **argv)
{
    int i, k, l;
    RegexpWithLook *rwl;
    Prog *prog;
    char *sub[MAXSUB];

    if (argc < 2)
        usage();

    rwl = parse(argv[1]);
    printre(rwl->regexp);
    printf("\n");

    prog = compile(rwl);
    printf("Final program:\n");
    printprog(prog);

    for (i = 2; i < argc; i++) {
        printf("#%d %s\n", i-1, argv[i]);
        memset(sub, 0, sizeof sub);

        if (!pikevm(prog, argv[i], sub, nelem(sub))) {
            printf("-no match-\n");
            continue;
        }

        printf("match");
        for (k = MAXSUB; k > 0; k--) {
            if (sub[k-1]) break;
        }

        for (l = 0; l < k; l += 2) {
            printf(" (");
            if (sub[l] == nil)
                printf("?");
            else
                printf("%d", (int)(sub[l] - argv[i]));

            printf(",");
            if (sub[l+1] == nil)
                printf("?");
            else
                printf("%d", (int)(sub[l+1] - argv[i]));
            printf(")");
        }

        printf("\n");
    }

    return 0;
}
