#ifndef SETTINGS_CATALOG_H
#define SETTINGS_CATALOG_H
#include <stdbool.h>
#include <stddef.h>
typedef enum { SETTINGS_CATEGORY, SETTINGS_DIRECTORY, SETTINGS_DETAIL } settings_node_kind_t;
typedef struct {
    const char *id, *parent, *title, *hint, *icon;
    settings_node_kind_t kind;
    int page;
    /* Visual group within parent; NULL gives an independent group. */
    const char *group;
} settings_node_t;
/* Immutable definitions must outlive the view; array order is display order, never identity. */
const settings_node_t *settings_catalog(size_t *count);
const settings_node_t *settings_catalog_find(const settings_node_t *,size_t,const char *id);
bool settings_catalog_validate(const settings_node_t *,size_t);
#endif
