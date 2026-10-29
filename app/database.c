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
  int result = sqlite3_open(path, &db);
  if (result == SQLITE_OK) result = sqlite3_exec(db, seed, NULL, NULL, NULL);
  sqlite3_close(db);
  return result == SQLITE_OK ? 0 : 1;
}

int database_equipment_json(const char *path, int equipment_id, char *output,
                            size_t output_size) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  const char *query =
      "SELECT e.equipment_id, e.name, e.description, e.condition, e.status, "
      "e.location, c.name FROM equipment e "
      "JOIN categories c ON c.category_id = e.category_id "
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
          "\"category\":\"%s\"}",
          rows == 0 ? "" : ",", sqlite3_column_int(statement, 0),
          (const char *)sqlite3_column_text(statement, 1),
          (const char *)sqlite3_column_text(statement, 2),
          (const char *)sqlite3_column_text(statement, 3),
          (const char *)sqlite3_column_text(statement, 4),
          (const char *)sqlite3_column_text(statement, 5),
          (const char *)sqlite3_column_text(statement, 6));
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
