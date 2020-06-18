#include "regexp.h"

typedef struct Transition Transition;
typedef struct TransitionFrag TransitionFrag;
typedef struct Pair Pair;
typedef struct StateList StateList;

struct Transition {
    StateList *from;
    TransitionFrag *to;

    Transition *next;
};

struct TransitionFrag {
    /* array of length nstates */
    StateList *to_states;

    Pair *pairs;

    TransitionFrag *next;
};

enum       /* Transition.label */
{
    Epsilon = 1,
    SubInfo,
    Input
};

struct Pair {
    int label;
    int info;

    Pair *next;
};

struct StateList {
    int *states;
    int len;
};

void add_transition(Transition**, StateList*, StateList*, Pair*);
int transition_exists(int, int, Prog*, Pair*, int);
Pair* get_transition(int, int, Prog*);
void remove_dead_states(Transition**, StateList*);

StateList* create_state_list(int);

void add_to_pair_list(Pair**, int, int);
void print_pair_list(Pair*);

void print_dot(Transition*, StateList*, StateList*);
void print_transition_table(Transition*);
void print_transition(StateList*, TransitionFrag*);
