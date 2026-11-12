#ifndef EQUIPMENT_LIBRARY_DATABASE_H
#define EQUIPMENT_LIBRARY_DATABASE_H

#include <stddef.h>

int database_initialize(const char *path);
int database_table_count(const char *path, int *count);
int database_seed_demo(const char *path);
int database_equipment_json(const char *path, int equipment_id, char *output,
                            size_t output_size);
int database_equipment_search(const char *path, const char *name,
                              const char *category, const char *condition,
                              const char *status, char *output,
                              size_t output_size, int *row_count);
int database_equipment_create(const char *path, const char *name,
                              const char *description, const char *category,
                              const char *condition, const char *status,
                              const char *location, int *equipment_id);
int database_equipment_update(const char *path, int equipment_id,
                              const char *name, const char *description,
                              const char *category, const char *condition,
                              const char *status, const char *location);
int database_equipment_delete(const char *path, int equipment_id);
int database_loan_create(const char *path, int equipment_id,
                         const char *borrower_name, const char *borrower_email,
                         const char *due_date, int *loan_id);
int database_loan_return(const char *path, int loan_id);
int database_maintenance_create(const char *path, int equipment_id,
                                const char *description, const char *reported_date,
                                double cost, int *maintenance_id);
int database_maintenance_update(const char *path, int maintenance_id,
                                const char *description, double cost);
int database_maintenance_complete(const char *path, int maintenance_id);
int database_maintenance_json(const char *path, char *output, size_t output_size);

#endif
