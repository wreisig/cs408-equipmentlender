#ifndef EQUIPMENT_LIBRARY_DATABASE_H
#define EQUIPMENT_LIBRARY_DATABASE_H

int database_initialize(const char *path);
int database_table_count(const char *path, int *count);

#endif
