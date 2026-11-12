#include "database.h"
#include <stddef.h>
#include <sqlite3.h>
#include <stdio.h>

static const char *schema_sql =
    "PRAGMA foreign_keys = ON;"
    "BEGIN;"
    "CREATE TABLE IF NOT EXISTS categories ("
    "category_id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, description TEXT);"
    "CREATE TABLE IF NOT EXISTS equipment ("
    "equipment_id INTEGER PRIMARY KEY, category_id INTEGER NOT NULL, name TEXT NOT NULL,"
    "description TEXT, serial_number TEXT UNIQUE, condition TEXT NOT NULL,"
    "status TEXT NOT NULL DEFAULT 'Available', location TEXT, purchase_date TEXT,"
    "created_at TEXT NOT NULL, updated_at TEXT NOT NULL,"
    "FOREIGN KEY (category_id) REFERENCES categories(category_id));"
    "CREATE TABLE IF NOT EXISTS borrowers ("
    "borrower_id INTEGER PRIMARY KEY, name TEXT NOT NULL, email TEXT NOT NULL UNIQUE,"
    "phone TEXT, active INTEGER NOT NULL DEFAULT 1, created_at TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS loans ("
    "loan_id INTEGER PRIMARY KEY, equipment_id INTEGER NOT NULL, borrower_id INTEGER NOT NULL,"
    "checkout_date TEXT NOT NULL, due_date TEXT NOT NULL, return_date TEXT,"
    "status TEXT NOT NULL, notes TEXT,"
    "FOREIGN KEY (equipment_id) REFERENCES equipment(equipment_id),"
    "FOREIGN KEY (borrower_id) REFERENCES borrowers(borrower_id));"
    "CREATE TABLE IF NOT EXISTS maintenance_records ("
    "maintenance_id INTEGER PRIMARY KEY, equipment_id INTEGER NOT NULL,"
    "description TEXT NOT NULL, reported_date TEXT NOT NULL, completed_date TEXT,"
    "status TEXT NOT NULL, cost REAL,"
    "FOREIGN KEY (equipment_id) REFERENCES equipment(equipment_id));"
    "CREATE INDEX IF NOT EXISTS idx_equipment_category ON equipment(category_id);"
    "CREATE INDEX IF NOT EXISTS idx_equipment_status ON equipment(status);"
    "CREATE INDEX IF NOT EXISTS idx_loans_equipment ON loans(equipment_id);"
    "CREATE INDEX IF NOT EXISTS idx_loans_borrower ON loans(borrower_id);"
    "CREATE INDEX IF NOT EXISTS idx_maintenance_equipment "
    "ON maintenance_records(equipment_id);"
    "PRAGMA user_version = 2;"
    "COMMIT;";

int database_initialize(const char *path) {
  sqlite3 *db = NULL;
  char *error_message = NULL;
  int result = sqlite3_open(path, &db);
  if (result == SQLITE_OK) result = sqlite3_busy_timeout(db, 5000);
  if (result == SQLITE_OK) {
    result = sqlite3_exec(db, schema_sql, NULL, NULL, &error_message);
  }
  if (result != SQLITE_OK && error_message != NULL) sqlite3_free(error_message);
  sqlite3_close(db);
  return result == SQLITE_OK ? 0 : 1;
}

int database_table_count(const char *path, int *count) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  if (result == SQLITE_OK) {
    result = sqlite3_prepare_v2(
        db,
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name IN "
        "('categories', 'equipment', 'borrowers', 'loans', 'maintenance_records');",
        -1, &statement, NULL);
  }
  if (result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
    *count = sqlite3_column_int(statement, 0);
  } else if (result == SQLITE_OK) {
    result = SQLITE_ERROR;
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return result == SQLITE_OK ? 0 : 1;
}

int database_seed_demo(const char *path) {
  sqlite3 *db = NULL;
  const char *seed =
      "INSERT OR IGNORE INTO categories(category_id, name, description) VALUES "
      "(1, 'Audio', 'Synthesizers and audio equipment'),"
      "(2, 'Sports', 'Sports and recreation equipment'),"
      "(3, 'Technology', 'Computers and electronics');"
      "INSERT OR IGNORE INTO equipment"
      "(equipment_id, category_id, name, description, serial_number, condition, "
      "status, location, created_at, updated_at) VALUES "
      "(1, 1, 'Analog Synthesizer', 'Compact keyboard synthesizer', 'SYN-001', "
      "'Good', 'Available', 'Music Room', datetime('now'), datetime('now')),"
      "(2, 2, 'Tennis Racquet', 'Standard adult tennis racquet', 'RAC-001', "
      "'Good', 'Borrowed', 'Sports Closet', datetime('now'), datetime('now')),"
      "(3, 3, 'Laptop Computer', 'Laptop for coursework and presentations', 'LAP-001', "
      "'Fair', 'Maintenance', 'IT Workbench', datetime('now'), datetime('now'));";
  const char *loan_seed =
      "INSERT OR IGNORE INTO borrowers"
      "(borrower_id, name, email, active, created_at) VALUES "
      "(1, 'Alex Morgan', 'alex.morgan@example.com', 1, datetime('now'));"
      "INSERT OR IGNORE INTO loans"
      "(loan_id, equipment_id, borrower_id, checkout_date, due_date, status, notes) VALUES "
      "(1, 2, 1, date('now', '-2 day'), date('now', '+5 day'), 'Active', "
      "'Return to the sports closet when complete.');";
  int result = sqlite3_open(path, &db);
  if (result == SQLITE_OK) result = sqlite3_exec(db, seed, NULL, NULL, NULL);
  if (result == SQLITE_OK) result = sqlite3_exec(db, loan_seed, NULL, NULL, NULL);
  sqlite3_close(db);
  return result == SQLITE_OK ? 0 : 1;
}

int database_equipment_json(const char *path, int equipment_id, char *output,
                            size_t output_size) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  const char *query =
      "SELECT e.equipment_id, e.name, e.description, e.condition, e.status, "
      "e.location, c.name, b.name, l.status, l.due_date FROM equipment e "
      "JOIN categories c ON c.category_id = e.category_id "
      "LEFT JOIN loans l ON l.equipment_id = e.equipment_id AND l.return_date IS NULL "
      "LEFT JOIN borrowers b ON b.borrower_id = l.borrower_id "
      "WHERE (? = 0 OR e.equipment_id = ?) ORDER BY e.equipment_id;";
  int result = sqlite3_open(path, &db);
  size_t used = 0;
  int rows = 0;
  if (result == SQLITE_OK) result = sqlite3_prepare_v2(db, query, -1, &statement, NULL);
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, equipment_id);
    sqlite3_bind_int(statement, 2, equipment_id);
    used = (size_t)snprintf(output, output_size, "[");
    while (sqlite3_step(statement) == SQLITE_ROW) {
      int written = snprintf(
          output + used, output_size - used,
          "%s{\"id\":%d,\"name\":\"%s\",\"description\":\"%s\","
          "\"condition\":\"%s\",\"status\":\"%s\",\"location\":\"%s\","
          "\"category\":\"%s\",\"borrower\":\"%s\",\"loan_status\":\"%s\","
          "\"due_date\":\"%s\"}",
          rows == 0 ? "" : ",", sqlite3_column_int(statement, 0),
          (const char *)sqlite3_column_text(statement, 1),
          (const char *)sqlite3_column_text(statement, 2),
          (const char *)sqlite3_column_text(statement, 3),
          (const char *)sqlite3_column_text(statement, 4),
          (const char *)sqlite3_column_text(statement, 5),
          (const char *)sqlite3_column_text(statement, 6),
          sqlite3_column_type(statement, 7) == SQLITE_NULL ? "" :
              (const char *)sqlite3_column_text(statement, 7),
          sqlite3_column_type(statement, 8) == SQLITE_NULL ? "" :
              (const char *)sqlite3_column_text(statement, 8),
          sqlite3_column_type(statement, 9) == SQLITE_NULL ? "" :
              (const char *)sqlite3_column_text(statement, 9));
      if (written < 0 || (size_t)written >= output_size - used) {
        result = SQLITE_TOOBIG;
        break;
      }
      used += (size_t)written;
      rows++;
    }
    if (result == SQLITE_OK) {
      if (used + 2 > output_size) result = SQLITE_TOOBIG;
      else (void)snprintf(output + used, output_size - used, "]");
    }
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  if (result != SQLITE_OK) return 1;
  return rows == 0 && equipment_id != 0 ? 2 : 0;
}

int database_equipment_search(const char *path, const char *name,
                              const char *category, const char *condition,
                              const char *status, char *output,
                              size_t output_size, int *row_count) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  const char *query =
      "SELECT e.equipment_id, e.name, e.description, e.condition, e.status, "
      "e.location, c.name, COALESCE(b.name, ''), COALESCE(l.status, ''), "
      "COALESCE(l.due_date, '') FROM equipment e "
      "JOIN categories c ON c.category_id = e.category_id "
      "LEFT JOIN loans l ON l.equipment_id = e.equipment_id AND l.return_date IS NULL "
      "LEFT JOIN borrowers b ON b.borrower_id = l.borrower_id "
      "WHERE e.name LIKE ? COLLATE NOCASE "
      "AND c.name LIKE ? COLLATE NOCASE "
      "AND e.condition LIKE ? COLLATE NOCASE "
      "AND e.status LIKE ? COLLATE NOCASE "
      "ORDER BY e.equipment_id;";
  char name_filter[256], category_filter[256], condition_filter[256],
      status_filter[256];
  int result = sqlite3_open(path, &db);
  size_t used = 0;
  int rows = 0;
  if (result != SQLITE_OK) return 1;
  (void)snprintf(name_filter, sizeof(name_filter), "%%%s%%", name);
  (void)snprintf(category_filter, sizeof(category_filter), "%%%s%%", category);
  (void)snprintf(condition_filter, sizeof(condition_filter), "%%%s%%", condition);
  (void)snprintf(status_filter, sizeof(status_filter), "%%%s%%", status);
  result = sqlite3_prepare_v2(db, query, -1, &statement, NULL);
  if (result == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, name_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, category_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, condition_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, status_filter, -1, SQLITE_TRANSIENT);
    used = (size_t)snprintf(output, output_size, "[");
    while (sqlite3_step(statement) == SQLITE_ROW) {
      int written = snprintf(
          output + used, output_size - used,
          "%s{\"id\":%d,\"name\":\"%s\",\"description\":\"%s\","
          "\"condition\":\"%s\",\"status\":\"%s\",\"location\":\"%s\","
          "\"category\":\"%s\",\"borrower\":\"%s\",\"loan_status\":\"%s\","
          "\"due_date\":\"%s\"}",
          rows == 0 ? "" : ",", sqlite3_column_int(statement, 0),
          (const char *)sqlite3_column_text(statement, 1),
          (const char *)sqlite3_column_text(statement, 2),
          (const char *)sqlite3_column_text(statement, 3),
          (const char *)sqlite3_column_text(statement, 4),
          (const char *)sqlite3_column_text(statement, 5),
          (const char *)sqlite3_column_text(statement, 6),
          (const char *)sqlite3_column_text(statement, 7),
          (const char *)sqlite3_column_text(statement, 8),
          (const char *)sqlite3_column_text(statement, 9));
      if (written < 0 || (size_t)written >= output_size - used) {
        result = SQLITE_TOOBIG;
        break;
      }
      used += (size_t)written;
      rows++;
    }
    if (result == SQLITE_OK) {
      if (used + 2 > output_size) result = SQLITE_TOOBIG;
      else (void)snprintf(output + used, output_size - used, "]");
    }
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  *row_count = rows;
  return result == SQLITE_OK ? 0 : 1;
}

static int category_id(sqlite3 *db, const char *name, int create) {
  sqlite3_stmt *statement = NULL;
  int id = 0;
  if (create && sqlite3_prepare_v2(db,
      "INSERT OR IGNORE INTO categories(name, description) VALUES (?, ?);",
      -1, &statement, NULL) == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, "Equipment category", -1, SQLITE_STATIC);
    (void)sqlite3_step(statement);
  }
  sqlite3_finalize(statement);
  statement = NULL;
  if (sqlite3_prepare_v2(db, "SELECT category_id FROM categories WHERE name = ?;",
                         -1, &statement, NULL) == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW) id = sqlite3_column_int(statement, 0);
  }
  sqlite3_finalize(statement);
  return id;
}

int database_equipment_create(const char *path, const char *name,
                              const char *description, const char *category,
                              const char *condition, const char *status,
                              const char *location, int *equipment_id) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  int category_value;
  if (result != SQLITE_OK) return 1;
  category_value = category_id(db, category, 1);
  if (category_value == 0 || sqlite3_prepare_v2(db,
      "INSERT INTO equipment(category_id,name,description,condition,status,location,"
      "created_at,updated_at) VALUES (?,?,?,?,?,?,datetime('now'),datetime('now'));",
      -1, &statement, NULL) != SQLITE_OK) result = SQLITE_ERROR;
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, category_value);
    sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, condition, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, location, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement);
    if (result == SQLITE_DONE) *equipment_id = (int)sqlite3_last_insert_rowid(db);
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return result == SQLITE_DONE ? 0 : 1;
}

int database_equipment_update(const char *path, int equipment_id,
                              const char *name, const char *description,
                              const char *category, const char *condition,
                              const char *status, const char *location) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  int category_value;
  int changed = 0;
  if (result != SQLITE_OK) return 1;
  category_value = category_id(db, category, 1);
  if (category_value == 0 || sqlite3_prepare_v2(db,
      "UPDATE equipment SET category_id=?,name=?,description=?,condition=?,status=?,"
      "location=?,updated_at=datetime('now') WHERE equipment_id=?;",
      -1, &statement, NULL) != SQLITE_OK) result = SQLITE_ERROR;
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, category_value);
    sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, condition, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, location, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 7, equipment_id);
    result = sqlite3_step(statement);
    changed = sqlite3_changes(db);
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return result == SQLITE_DONE && changed == 1 ? 0 : 1;
}

int database_equipment_delete(const char *path, int equipment_id) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  if (result == SQLITE_OK) result = sqlite3_prepare_v2(db,
      "DELETE FROM equipment WHERE equipment_id=?;", -1, &statement, NULL);
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, equipment_id);
    result = sqlite3_step(statement);
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return result == SQLITE_DONE ? 0 : 1;
}

int database_loan_create(const char *path, int equipment_id,
                         const char *borrower_name, const char *borrower_email,
                         const char *due_date, int *loan_id) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  int borrower_id = 0;
  if (result != SQLITE_OK) return 1;
  if (sqlite3_prepare_v2(db,
      "SELECT borrower_id FROM borrowers WHERE email=?;", -1, &statement, NULL) == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, borrower_email, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW) borrower_id = sqlite3_column_int(statement, 0);
  }
  sqlite3_finalize(statement);
  statement = NULL;
  if (borrower_id == 0 && sqlite3_prepare_v2(db,
      "INSERT INTO borrowers(name,email,created_at) VALUES (?,?,datetime('now'));",
      -1, &statement, NULL) == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, borrower_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, borrower_email, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement);
    if (result == SQLITE_DONE) borrower_id = (int)sqlite3_last_insert_rowid(db);
  }
  sqlite3_finalize(statement);
  statement = NULL;
  result = SQLITE_OK;
  if (borrower_id == 0 || sqlite3_prepare_v2(db,
      "INSERT INTO loans(equipment_id,borrower_id,checkout_date,due_date,status) "
      "SELECT ?,?,date('now'),?,'Active' WHERE EXISTS "
      "(SELECT 1 FROM equipment WHERE equipment_id=? AND status='Available');",
      -1, &statement, NULL) != SQLITE_OK) result = SQLITE_ERROR;
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, equipment_id);
    sqlite3_bind_int(statement, 2, borrower_id);
    sqlite3_bind_text(statement, 3, due_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, equipment_id);
    result = sqlite3_step(statement);
    if (result == SQLITE_DONE && sqlite3_changes(db) == 1) {
      *loan_id = (int)sqlite3_last_insert_rowid(db);
      sqlite3_finalize(statement);
      statement = NULL;
      if (sqlite3_prepare_v2(db, "UPDATE equipment SET status='Borrowed',updated_at=datetime('now') WHERE equipment_id=?;",
                             -1, &statement, NULL) == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, equipment_id);
        result = sqlite3_step(statement);
      }
    } else result = SQLITE_CONSTRAINT;
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return result == SQLITE_DONE ? 0 : 1;
}

int database_loan_return(const char *path, int loan_id) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  int changed = 0;
  if (result == SQLITE_OK) result = sqlite3_prepare_v2(db,
      "UPDATE loans SET status='Returned',return_date=date('now') "
      "WHERE loan_id=? AND return_date IS NULL;", -1, &statement, NULL);
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, loan_id);
    result = sqlite3_step(statement);
    changed = sqlite3_changes(db);
  }
  sqlite3_finalize(statement);
  if (result == SQLITE_DONE && changed == 1) {
    statement = NULL;
    if (sqlite3_prepare_v2(db,
        "UPDATE equipment SET status='Available',updated_at=datetime('now') "
        "WHERE equipment_id=(SELECT equipment_id FROM loans WHERE loan_id=?);",
        -1, &statement, NULL) == SQLITE_OK) {
      sqlite3_bind_int(statement, 1, loan_id);
      (void)sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
  }
  sqlite3_close(db);
  return result == SQLITE_DONE && changed == 1 ? 0 : 1;
}

int database_loan_return_equipment(const char *path, int equipment_id) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  int loan_id = 0;
  if (result == SQLITE_OK) result = sqlite3_prepare_v2(db,
      "SELECT loan_id FROM loans WHERE equipment_id=? AND return_date IS NULL "
      "ORDER BY loan_id DESC LIMIT 1;", -1, &statement, NULL);
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, equipment_id);
    if (sqlite3_step(statement) == SQLITE_ROW) loan_id = sqlite3_column_int(statement, 0);
    else result = SQLITE_NOTFOUND;
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return result == SQLITE_OK && loan_id > 0 ? database_loan_return(path, loan_id) : 1;
}

int database_maintenance_create(const char *path, int equipment_id,
                                const char *description, const char *reported_date,
                                double cost, int *maintenance_id) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  if (result == SQLITE_OK) result = sqlite3_prepare_v2(db,
      "INSERT INTO maintenance_records(equipment_id,description,reported_date,status,cost) "
      "VALUES (?,?,?,'Open',?);", -1, &statement, NULL);
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, equipment_id);
    sqlite3_bind_text(statement, 2, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, reported_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 4, cost);
    result = sqlite3_step(statement);
    if (result == SQLITE_DONE) *maintenance_id = (int)sqlite3_last_insert_rowid(db);
  }
  sqlite3_finalize(statement);
  if (result == SQLITE_DONE) sqlite3_exec(db,
      "UPDATE equipment SET status='Maintenance',updated_at=datetime('now') "
      "WHERE equipment_id=(SELECT equipment_id FROM maintenance_records "
      "WHERE maintenance_id=last_insert_rowid());", NULL, NULL, NULL);
  sqlite3_close(db);
  return result == SQLITE_DONE ? 0 : 1;
}

int database_maintenance_update(const char *path, int maintenance_id,
                                const char *description, double cost) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  int changed = 0;
  if (result == SQLITE_OK) result = sqlite3_prepare_v2(db,
      "UPDATE maintenance_records SET description=?,cost=? WHERE maintenance_id=?;",
      -1, &statement, NULL);
  if (result == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 2, cost);
    sqlite3_bind_int(statement, 3, maintenance_id);
    result = sqlite3_step(statement);
    changed = sqlite3_changes(db);
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return result == SQLITE_DONE && changed == 1 ? 0 : 1;
}

int database_maintenance_complete(const char *path, int maintenance_id) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  int result = sqlite3_open(path, &db);
  int changed = 0;
  if (result == SQLITE_OK) result = sqlite3_prepare_v2(db,
      "UPDATE maintenance_records SET status='Completed',completed_date=date('now') "
      "WHERE maintenance_id=?;", -1, &statement, NULL);
  if (result == SQLITE_OK) {
    sqlite3_bind_int(statement, 1, maintenance_id);
    result = sqlite3_step(statement);
    changed = sqlite3_changes(db);
  }
  sqlite3_finalize(statement);
  if (result == SQLITE_DONE && changed == 1) {
    statement = NULL;
    if (sqlite3_prepare_v2(db,
        "UPDATE equipment SET status='Available',updated_at=datetime('now') "
        "WHERE equipment_id=(SELECT equipment_id FROM maintenance_records "
        "WHERE maintenance_id=?);", -1, &statement, NULL) == SQLITE_OK) {
      sqlite3_bind_int(statement, 1, maintenance_id);
      (void)sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
  }
  sqlite3_close(db);
  return result == SQLITE_DONE && changed == 1 ? 0 : 1;
}

int database_maintenance_json(const char *path, char *output, size_t output_size) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  size_t used = 0;
  int rows = 0;
  int result = sqlite3_open(path, &db);
  if (result == SQLITE_OK) result = sqlite3_prepare_v2(db,
      "SELECT maintenance_id,equipment_id,description,reported_date,"
      "COALESCE(completed_date,''),status,COALESCE(cost,0) FROM maintenance_records "
      "ORDER BY maintenance_id;", -1, &statement, NULL);
  used = (size_t)snprintf(output, output_size, "[");
  while (result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
    int written = snprintf(output + used, output_size - used,
      "%s{\"id\":%d,\"equipment_id\":%d,\"description\":\"%s\",\"reported_date\":\"%s\","
      "\"completed_date\":\"%s\",\"status\":\"%s\",\"cost\":%.2f}",
      rows++ == 0 ? "" : ",", sqlite3_column_int(statement, 0),
      sqlite3_column_int(statement, 1), sqlite3_column_text(statement, 2),
      sqlite3_column_text(statement, 3), sqlite3_column_text(statement, 4),
      sqlite3_column_text(statement, 5), sqlite3_column_double(statement, 6));
    if (written < 0 || (size_t)written >= output_size - used) result = SQLITE_TOOBIG;
    else used += (size_t)written;
  }
  if (result == SQLITE_OK && used + 2 <= output_size) snprintf(output + used, output_size - used, "]");
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return result == SQLITE_OK ? 0 : 1;
}
