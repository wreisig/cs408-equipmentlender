#ifndef EQUIPMENT_LIBRARY_DATABASE_H
#define EQUIPMENT_LIBRARY_DATABASE_H

#include <stddef.h>

int database_initialize(const char *path);
int database_table_count(const char *path, int *count);
int database_seed_demo(const char *path);
int database_equipment_json(const char *path, int equipment_id, char *output,
                            size_t output_size);

#endif
