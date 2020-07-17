#include <glib.h>
#include "regex.h"
#include "hashset.h"

typedef struct FinalStates FinalStates;
typedef struct Alphabet Alphabet;
typedef struct StateList StateList;
typedef struct CollectionStateList CollectionStateList;
typedef struct TransitionTable TransitionTable;
typedef struct TransitionLabel TransitionLabel;
typedef struct StateAndTransitionLabel StateAndTransitionLabel;

enum       /* TransitionLabel.label */
{
    Epsilon = 1,
    Input
};

struct FinalStates {
    HashSet *states;
};
FinalStates* create_final_states();

struct StateList {
    int *states;
    int len;
};
guint state_list_hash(gconstpointer key);
gboolean state_list_eq(gconstpointer, gconstpointer);

struct CollectionStateList {
    HashSet *state_sets;
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

struct StateAndTransitionLabel {
    int state;
    TransitionLabel *tl;
};
guint state_and_tl_hash(gconstpointer);
gboolean state_and_tl_eq(gconstpointer, gconstpointer);

struct Alphabet {
    GHashTable *ht;
};

TransitionTable* transition_table_new();
void transition_table_free(TransitionTable*);

int path_exists(int, int, Prog*, TransitionLabel*, int);

void add_sl_transition(TransitionTable*, StateList*, StateList*, TransitionLabel*);

Prog* convert_to_prog(TransitionTable*, TransitionTable*, StateList*, FinalStates*, GHashTable*);

TransitionLabel* make_epsilon_tl();
TransitionLabel* make_char_tl(int);
TransitionLabel* make_transition_label(int, int);
StateAndTransitionLabel* make_state_and_tl(int, TransitionLabel*);

void add_to_alphabet(Alphabet**, int, int);
void print_alphabet(Alphabet*);

void print_dot(TransitionTable*, StateList*, FinalStates*);

StateList* create_state_list(int);
int state_list_equals(StateList*, StateList*);
void print_state_list(StateList*);
