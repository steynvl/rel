#include "utils.h"

ProgWithLook* create_prog_with_look(int num_progs, int *sizes)
{
    ProgWithLook *pwl;
    Prog *p;
    int i;

    pwl = umal(num_progs * sizeof(ProgWithLook));
    pwl->len = num_progs;
    for (i = 0; i < num_progs; i++) {
        p = imal(sizeof *p + sizes[i]*sizeof p->start[0], 1);
        p->start = (Inst*)(p+1);
        p->len = sizes[i] - 1;
        pwl[i].prog = p;
    }

    return pwl;
}

int** create_2d_arr(int r, int c)
{
    int **arr;
    int i;

    arr = umal(r * sizeof(int*));
    for (i = 0; i < r; i++) {
        arr[i] = umal(c * sizeof(int));
    }

    return arr;
}

void print_states(int **states, int r, int c, int *offsets)
{
    int i, j;

    for (i = 0; i < r; i++) {
        printf("(");
        for (j = 0; j < c; j++) {
            printf("%d", states[i][j] + offsets[j]);
            if (j != c - 1) {
                printf(", ");
            }
        }
        printf(")\n");
    }
}

