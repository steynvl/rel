#include <glib.h>
#include "regexp.h"

typedef struct Transition Transition;
typedef struct TransitionFrag TransitionFrag;
typedef struct Alphabet Alphabet;
typedef struct StateList StateList;
typedef struct CollectionStateList CollectionStateList;
typedef struct TransitionTable TransitionTable;
typedef struct TransitionLabel TransitionLabel;

enum       /* TransitionLabel.label */
{
    Epsilon = 1,
    SubInfo,
    Input
};

struct StateList {
    int *states;
    int len;
};
guint state_list_hash(gconstpointer key);
gboolean state_list_eq(gconstpointer, gconstpointer);

struct CollectionStateList {
    StateList **state_lists;
    int len;
};
guint collec_state_list_hash(gconstpointer);
gboolean collec_state_list_eq(gconstpointer, gconstpointer);

struct TransitionTable {
    GHashTable *ht;
};

struct TransitionLabel {
    int label;
    int info;
};
guint transition_label_hash(gconstpointer);
gboolean transition_label_eq(gconstpointer, gconstpointer);

struct Alphabet {
    int label;
    int info;
    Alphabet *next;
};

TransitionTable* transition_table_new();
void transition_table_free(TransitionTable*);

int path_exists(int, int, Prog*, Alphabet*, int);

void add_sl_transition(TransitionTable*, StateList*, StateList*, TransitionLabel*);

Prog* convert_to_prog(TransitionTable*, StateList*, StateList*);

TransitionLabel* make_epsilon_tl();
TransitionLabel* make_char_tl(int);
TransitionLabel* make_transition_label(int, int);

void add_to_alphabet(Alphabet**, int, int);
void print_alphabet(Alphabet*);

void print_dot(TransitionTable*, StateList*, StateList*);

StateList* create_state_list(int);
int state_list_equals(StateList*, StateList*);
