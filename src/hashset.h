#include <glib.h>

typedef struct HashSet HashSet;
typedef struct HashSetIter HashSetIter;

struct HashSet {
    GHashTable *ht;
};

struct HashSetIter {
    GHashTableIter iter;
};

HashSet* hash_set_new(GHashFunc hash_func, GEqualFunc key_equal_func);
gboolean hash_set_add(HashSet *hs, gpointer element);
void hash_set_add_all(HashSet *hs, HashSet *to_add);
gboolean hash_set_contains(HashSet *hs, gconstpointer key);
guint hash_set_size(HashSet *hs);
void hash_set_iter_init(HashSetIter *iter, HashSet *hs);
gboolean hash_set_iter_next(HashSetIter *iter, gpointer *key);
GQueue* hash_set_to_g_queue(HashSet *hs);
GPtrArray* hash_set_to_g_ptr_array(HashSet *hs);

