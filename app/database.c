#include "database.h"
#include <stddef.h>
#include <sqlite3.h>

static const char *schema_sql =
    "PRAGMA foreign_keys = ON;"
    "BEGIN;"
    "CREATE TABLE IF NOT EXISTS categories ("
    "category_id INTEGER PRIMARY KEY,"
    "name TEXT NOT NULL UNIQUE,"
    "description TEXT);"
    "CREATE TABLE IF NOT EXISTS equipment ("
    "equipment_id INTEGER PRIMARY KEY,"
    "category_id INTEGER NOT NULL,"
    "name TEXT NOT NULL,"
    "description TEXT,"
    "serial_number TEXT UNIQUE,"
    "condition TEXT NOT NULL,"
    "status TEXT NOT NULL DEFAULT 'Available',"
    "location TEXT,"
    "purchase_date TEXT,"
    "created_at TEXT NOT NULL,"
    "updated_at TEXT NOT NULL,"
    "FOREIGN KEY (category_id) REFERENCES categories(category_id));"
    "CREATE TABLE IF NOT EXISTS borrowers ("
    "borrower_id INTEGER PRIMARY KEY,"
    "name TEXT NOT NULL,"
    "email TEXT NOT NULL UNIQUE,"
    "phone TEXT,"
    "active INTEGER NOT NULL DEFAULT 1,"
    "created_at TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS loans ("
    "loan_id INTEGER PRIMARY KEY,"
    "equipment_id INTEGER NOT NULL,"
    "borrower_id INTEGER NOT NULL,"
    "checkout_date TEXT NOT NULL,"
    "due_date TEXT NOT NULL,"
    "return_date TEXT,"
    "status TEXT NOT NULL,"
    "notes TEXT,"
    "FOREIGN KEY (equipment_id) REFERENCES equipment(equipment_id),"
    "FOREIGN KEY (borrower_id) REFERENCES borrowers(borrower_id));"
    "CREATE TABLE IF NOT EXISTS maintenance_records ("
    "maintenance_id INTEGER PRIMARY KEY,"
    "equipment_id INTEGER NOT NULL,"
    "description TEXT NOT NULL,"
    "reported_date TEXT NOT NULL,"
    "completed_date TEXT,"
    "status TEXT NOT NULL,"
    "cost REAL,"
    "FOREIGN KEY (equipment_id) REFERENCES equipment(equipment_id));"
    "CREATE INDEX IF NOT EXISTS idx_equipment_category "
    "ON equipment(category_id);"
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
  if (result == SQLITE_OK) {
    result = sqlite3_busy_timeout(db, 5000);
  }
  if (result == SQLITE_OK) {
    result = sqlite3_exec(db, schema_sql, NULL, NULL, &error_message);
  }
  if (result != SQLITE_OK && error_message != NULL) {
    sqlite3_free(error_message);
  }
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
        "SELECT COUNT(*) FROM sqlite_master "
        "WHERE type = 'table' AND name IN "
        "('categories', 'equipment', 'borrowers', 'loans', "
        "'maintenance_records');",
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
