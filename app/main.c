#include "mongoose.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

static int initialize_database(const char *path) {
  sqlite3 *db = NULL;
  const char *schema =
      "PRAGMA foreign_keys = ON;"
      "CREATE TABLE IF NOT EXISTS categories ("
      "category_id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, "
      "description TEXT);"
      "CREATE TABLE IF NOT EXISTS equipment ("
      "equipment_id INTEGER PRIMARY KEY, category_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, description TEXT, serial_number TEXT UNIQUE, "
      "condition TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'Available', "
      "location TEXT, purchase_date TEXT, created_at TEXT NOT NULL, "
      "updated_at TEXT NOT NULL, FOREIGN KEY (category_id) "
      "REFERENCES categories(category_id));"
      "CREATE TABLE IF NOT EXISTS borrowers ("
      "borrower_id INTEGER PRIMARY KEY, name TEXT NOT NULL, "
      "email TEXT NOT NULL UNIQUE, phone TEXT, active INTEGER NOT NULL DEFAULT 1, "
      "created_at TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS loans ("
      "loan_id INTEGER PRIMARY KEY, equipment_id INTEGER NOT NULL, "
      "borrower_id INTEGER NOT NULL, checkout_date TEXT NOT NULL, "
      "due_date TEXT NOT NULL, return_date TEXT, status TEXT NOT NULL, notes TEXT, "
      "FOREIGN KEY (equipment_id) REFERENCES equipment(equipment_id), "
      "FOREIGN KEY (borrower_id) REFERENCES borrowers(borrower_id));"
      "CREATE TABLE IF NOT EXISTS maintenance_records ("
      "maintenance_id INTEGER PRIMARY KEY, equipment_id INTEGER NOT NULL, "
      "description TEXT NOT NULL, reported_date TEXT NOT NULL, "
      "completed_date TEXT, status TEXT NOT NULL, cost REAL, "
      "FOREIGN KEY (equipment_id) REFERENCES equipment(equipment_id));";
  int result = sqlite3_open(path, &db);
  if (result == SQLITE_OK) result = sqlite3_exec(db, schema, NULL, NULL, NULL);
  sqlite3_close(db);
  return result == SQLITE_OK ? 0 : 1;
}

static void http_handler(struct mg_connection *connection, int event,
                         void *event_data) {
  if (event != MG_EV_HTTP_MSG) return;
  struct mg_http_message *message = (struct mg_http_message *)event_data;
  if (mg_match(message->uri, mg_str("/api/health"), NULL)) {
    mg_http_reply(connection, 200, "Content-Type: application/json\r\n",
                  "{\"status\":\"ok\"}\n");
  } else if (mg_match(message->uri, mg_str("/"), NULL)) {
    mg_http_reply(connection, 200, "Content-Type: text/html; charset=utf-8\r\n",
                  "<!doctype html><html lang=\"en\"><head>"
                  "<meta charset=\"utf-8\"><meta name=\"viewport\" "
                  "content=\"width=device-width, initial-scale=1\">"
                  "<title>Equipment Lending Library</title>"
                  "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
                  "rel=\"stylesheet\"></head><body><main class=\"container py-5\">"
                  "<h1>Equipment Lending Library</h1>"
                  "<p class=\"lead\">Checkpoint 1 Home page</p>"
                  "<p>Browse and manage lending-library equipment from one place.</p>"
                  "</main></body></html>");
  } else {
    mg_http_reply(connection, 404, "", "Not found\n");
  }
}

int main(void) {
  const char *port = getenv("PORT");
  char listen_url[64];
  if (port == NULL) port = "8080";
  (void)snprintf(listen_url, sizeof(listen_url), "http://0.0.0.0:%s", port);
  if (initialize_database("data/equipment.db") != 0) {
    fprintf(stderr, "Could not initialize data/equipment.db\n");
    return EXIT_FAILURE;
  }
  struct mg_mgr manager;
  mg_mgr_init(&manager);
  if (mg_http_listen(&manager, listen_url, http_handler, NULL) == NULL) {
    fprintf(stderr, "Could not listen on %s\n", listen_url);
    mg_mgr_free(&manager);
    return EXIT_FAILURE;
  }
  printf("Equipment Lending Library running at http://localhost:%s/\n", port);
  fflush(stdout);
  for (;;) mg_mgr_poll(&manager, 1000);
  mg_mgr_free(&manager);
  return EXIT_SUCCESS;
}
