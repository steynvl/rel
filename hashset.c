#include "hashset.h"

HashSet* hash_set_new(GHashFunc hash_func, GEqualFunc key_equal_func)
{
    HashSet *hs;

    hs = umal(sizeof(HashSet));
    hs->ht = g_hash_table_new(hash_func, key_equal_func);
    return hs;
}

gboolean hash_set_add(HashSet* hs, gpointer element)
{
    return g_hash_table_add(hs->ht, element);
}

void hash_set_add_all(HashSet *hs, HashSet *to_add)
{
    HashSetIter iter;
    gpointer val;

    hash_set_iter_init(&iter, to_add);
    while (hash_set_iter_next(&iter, &val)) {
        hash_set_add(hs, val);
    }
}

gboolean hash_set_contains(HashSet *hs, gconstpointer key)
{
    return g_hash_table_contains(hs->ht, key);
}

void hash_set_iter_init(HashSetIter *iter, HashSet *hs)
{
    g_hash_table_iter_init(&iter->iter, hs->ht);
}

gboolean hash_set_iter_next(HashSetIter *iter, gpointer *key)
{
    return g_hash_table_iter_next(&iter->iter, key, key);
}

guint hash_set_size(HashSet *hs)
{
    return g_hash_table_size(hs->ht);
}

GQueue* hash_set_to_g_queue(HashSet *hs)
{
    HashSetIter iter;
    gpointer val;
    GQueue *queue;

    queue = g_queue_new();
    hash_set_iter_init(&iter, hs);
    while (hash_set_iter_next(&iter, &val)) {
        g_queue_push_head(queue, val);
    }

    return queue;
}

GPtrArray* hash_set_to_g_ptr_array(HashSet *hs)
{
    HashSetIter iter;
    gpointer val;
    GPtrArray *arr;
    guint len;

    arr = g_ptr_array_new();
    len = 0;

    hash_set_iter_init(&iter, hs);
    while (hash_set_iter_next(&iter, &val)) {
        g_ptr_array_add(arr, val);
        len++;
    }
    arr->len = len;

    return arr;
}
