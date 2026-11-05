#include "database.h"
#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int query_value(struct mg_http_message *message, const char *name,
                       char *value, size_t value_size) {
  int length = mg_http_get_var(&message->query, name, value, value_size);
  if (length < 0) value[0] = '\0';
  return length;
}

static int valid_filter(const char *value, const char *const *allowed) {
  size_t index;
  if (value[0] == '\0') return 1;
  for (index = 0; allowed[index] != NULL; index++) {
    if (strcmp(value, allowed[index]) == 0) return 1;
  }
  return 0;
}

static void send_search(struct mg_connection *connection,
                        struct mg_http_message *message) {
  char name[128], category[128], condition[64], status[64], json[16384];
  int row_count = 0;
  static const char *const conditions[] = {"New", "Good", "Fair", "Damaged", NULL};
  static const char *const statuses[] = {"Available", "Borrowed", "Maintenance",
                                         "Retired", NULL};
  (void)query_value(message, "name", name, sizeof(name));
  (void)query_value(message, "category", category, sizeof(category));
  (void)query_value(message, "condition", condition, sizeof(condition));
  (void)query_value(message, "status", status, sizeof(status));
  if (!valid_filter(condition, conditions) || !valid_filter(status, statuses)) {
    mg_http_reply(connection, 400, "Content-Type: application/json\r\n",
                  "{\"error\":\"invalid condition or status filter\"}\n");
    return;
  }
  if (database_equipment_search("data/equipment.db", name, category, condition,
                                status, json, sizeof(json), &row_count) != 0) {
    mg_http_reply(connection, 500, "Content-Type: application/json\r\n",
                  "{\"error\":\"database unavailable\"}\n");
  } else {
    mg_http_reply(connection, 200, "Content-Type: application/json\r\n",
                  "{\"results\":%s,\"count\":%d}\n", json, row_count);
  }
}

static void send_equipment(struct mg_connection *connection, int equipment_id) {
  char json[16384];
  int result = database_equipment_json("data/equipment.db", equipment_id, json,
                                       sizeof(json));
  if (result == 2) {
    mg_http_reply(connection, 404, "Content-Type: application/json\r\n",
                  "{\"error\":\"equipment not found\"}\n");
  } else if (result != 0) {
    mg_http_reply(connection, 500, "Content-Type: application/json\r\n",
                  "{\"error\":\"database unavailable\"}\n");
  } else {
    mg_http_reply(connection, 200, "Content-Type: application/json\r\n",
                  "%s\n", json);
  }
}

static int equipment_id_from_uri(struct mg_http_message *message) {
  char uri[128];
  size_t length = message->uri.len < sizeof(uri) - 1 ? message->uri.len
                                                      : sizeof(uri) - 1;
  char *last_slash;
  memcpy(uri, message->uri.buf, length);
  uri[length] = '\0';
  last_slash = strrchr(uri, '/');
  return last_slash == NULL ? 0 : atoi(last_slash + 1);
}

static void http_handler(struct mg_connection *connection, int event,
                         void *event_data) {
  if (event != MG_EV_HTTP_MSG) return;
  struct mg_http_message *message = (struct mg_http_message *)event_data;
  if (mg_match(message->uri, mg_str("/api/health"), NULL)) {
    mg_http_reply(connection, 200, "Content-Type: application/json\r\n",
                  "{\"status\":\"ok\"}\n");
  } else if (mg_match(message->uri, mg_str("/api/database"), NULL)) {
    int table_count = 0;
    if (database_table_count("data/equipment.db", &table_count) != 0) {
      mg_http_reply(connection, 500,
                    "Content-Type: application/json\r\n",
                    "{\"status\":\"error\",\"message\":\"database unavailable\"}\n");
      return;
    }
    mg_http_reply(connection, 200, "Content-Type: application/json\r\n",
                  "{\"status\":\"ok\",\"initial_tables\":%d}\n", table_count);
  } else if (mg_match(message->uri, mg_str("/api/equipment/search"), NULL)) {
    send_search(connection, message);
  } else if (mg_match(message->uri, mg_str("/api/equipment"), NULL)) {
    send_equipment(connection, 0);
  } else if (mg_match(message->uri, mg_str("/api/equipment/#"), NULL)) {
    send_equipment(connection, equipment_id_from_uri(message));
  } else if (mg_match(message->uri, mg_str("/equipment/#"), NULL)) {
    char json[16384];
    int equipment_id = equipment_id_from_uri(message);
    int result = database_equipment_json("data/equipment.db", equipment_id, json,
                                         sizeof(json));
    if (result == 2) {
      mg_http_reply(connection, 404, "Content-Type: text/plain\r\n",
                    "Equipment not found\n");
    } else if (result != 0) {
      mg_http_reply(connection, 500, "Content-Type: text/plain\r\n",
                    "Database unavailable\n");
    } else {
      mg_http_reply(connection, 200, "Content-Type: text/html; charset=utf-8\r\n",
                    "<!doctype html><html><head><meta charset=\"utf-8\">"
                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                    "<title>Equipment Details</title>"
                    "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
                    "rel=\"stylesheet\"></head><body><main class=\"container py-5\">"
                    "<h1>Equipment Details</h1><pre>%s</pre>"
                    "<a href=\"/equipment\">Back to catalog</a></main></body></html>", json);
    }
  } else if (mg_match(message->uri, mg_str("/equipment"), NULL)) {
    char json[16384];
    int row_count = 0;
    char name[128], category[128], condition[64], status[64];
    static const char *const conditions[] = {"New", "Good", "Fair", "Damaged", NULL};
    static const char *const statuses[] = {"Available", "Borrowed", "Maintenance",
                                           "Retired", NULL};
    (void)query_value(message, "name", name, sizeof(name));
    (void)query_value(message, "category", category, sizeof(category));
    (void)query_value(message, "condition", condition, sizeof(condition));
    (void)query_value(message, "status", status, sizeof(status));
    if (!valid_filter(condition, conditions) || !valid_filter(status, statuses) ||
        database_equipment_search("data/equipment.db", name, category, condition,
                                  status, json, sizeof(json), &row_count) != 0) {
      mg_http_reply(connection, 400, "Content-Type: text/plain\r\n",
                    "Invalid search filters or database unavailable\n");
      return;
    }
    mg_http_reply(connection, 200, "Content-Type: text/html; charset=utf-8\r\n",
                  "<!doctype html><html><head><meta charset=\"utf-8\">"
                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "<title>Equipment Catalog</title>"
                  "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
                  "rel=\"stylesheet\"></head><body><main class=\"container py-5\">"
                  "<h1>Equipment Catalog</h1><p>Browse the current equipment inventory "
                  "and availability.</p><form method=\"get\"><input name=\"name\" "
                  "placeholder=\"Search name\" value=\"%s\"><input name=\"category\" "
                  "placeholder=\"Category\" value=\"%s\"><select name=\"condition\">"
                  "<option value=\"\">Any condition</option><option>New</option>"
                  "<option>Good</option><option>Fair</option><option>Damaged</option>"
                  "</select><select name=\"status\"><option value=\"\">Any status</option>"
                  "<option>Available</option><option>Borrowed</option><option>Maintenance</option>"
                  "<option>Retired</option></select><button type=\"submit\">Filter</button>"
                  "</form><p><a href=\"/\">Home</a> | "
                  "<a href=\"/api/equipment\">View catalog data</a></p>"
                  "<div id=\"catalog\" class=\"row g-3\"></div><p id=\"empty\"></p><script>"
                  "const items=%s;const target=document.getElementById('catalog');"
                  "document.getElementById('empty').textContent=items.length===0?'No equipment matched your search.':'';"
                  "items.forEach(item=>{target.innerHTML+=`<div class=\"col-md-4\"><div class=\"card h-100\">"
                  "<div class=\"card-body\"><h2 class=\"h5\">${item.name}</h2><p>${item.description}</p>"
                  "<span class=\"badge text-bg-secondary\">${item.status}</span> "
                  "<span class=\"badge text-bg-info\">${item.category}</span>"
                  "<p class=\"mt-2 mb-0\">Condition: ${item.condition}<br>Location: ${item.location}</p>"
                  "${item.borrower ? `<p class=\"mb-0\">Borrower: ${item.borrower}<br>"
                  "Loan status: ${item.loan_status}<br>Due: ${item.due_date}</p>` : ''}"
                  "</div></div></div>`});</script></main></body></html>",
                  name, category, json);
  } else if (mg_match(message->uri, mg_str("/"), NULL)) {
    mg_http_reply(connection, 200, "Content-Type: text/html; charset=utf-8\r\n",
                  "<!doctype html><html lang=\"en\"><head>"
                  "<meta charset=\"utf-8\"><meta name=\"viewport\" "
                  "content=\"width=device-width, initial-scale=1\">"
                  "<title>Equipment Lending Library</title>"
                  "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
                  "rel=\"stylesheet\"></head><body><main class=\"container py-5\">"
                  "<h1>Equipment Lending Library</h1>"
                  "<p class=\"lead\">Database-backed application foundation</p>"
                  "<p>SQLite is initialized with the lending-library tables.</p>"
                  "<a class=\"btn btn-primary\" href=\"/equipment\">Browse equipment</a>"
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
  if (database_initialize("data/equipment.db") != 0) {
    fprintf(stderr, "Could not initialize data/equipment.db\n");
    return EXIT_FAILURE;
  }
  if (database_seed_demo("data/equipment.db") != 0) {
    fprintf(stderr, "Could not seed demo equipment data\n");
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
