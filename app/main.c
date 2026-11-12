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

static int body_value(struct mg_http_message *message, const char *name,
                      char *value, size_t value_size) {
  int length = mg_http_get_var(&message->body, name, value, value_size);
  if (length < 0) value[0] = '\0';
  return length;
}

static int required_values(const char *name, const char *category,
                           const char *condition, const char *status) {
  return name[0] != '\0' && category[0] != '\0' && condition[0] != '\0' &&
         status[0] != '\0';
}

static void operation_reply(struct mg_connection *connection, int result,
                            const char *success, const char *failure) {
  if (result == 0) {
    mg_http_reply(connection, 200, "Content-Type: application/json\r\n",
                  "{\"status\":\"ok\",\"message\":\"%s\"}\n", success);
  } else {
    mg_http_reply(connection, 400, "Content-Type: application/json\r\n",
                  "{\"status\":\"error\",\"message\":\"%s\"}\n", failure);
  }
}

static void create_equipment(struct mg_connection *connection,
                             struct mg_http_message *message) {
  char name[128], description[256], category[128], condition[64], status[64],
      location[128];
  int equipment_id = 0;
  (void)body_value(message, "name", name, sizeof(name));
  (void)body_value(message, "description", description, sizeof(description));
  (void)body_value(message, "category", category, sizeof(category));
  (void)body_value(message, "condition", condition, sizeof(condition));
  (void)body_value(message, "status", status, sizeof(status));
  (void)body_value(message, "location", location, sizeof(location));
  if (!required_values(name, category, condition, status)) {
    operation_reply(connection, 400, "", "name, category, condition, and status are required");
    return;
  }
  if (database_equipment_create("data/equipment.db", name, description, category,
                                condition, status, location, &equipment_id) != 0) {
    operation_reply(connection, 400, "", "equipment could not be created");
    return;
  }
  mg_http_reply(connection, 201, "Content-Type: application/json\r\n",
                "{\"status\":\"ok\",\"id\":%d,\"message\":\"equipment created\"}\n",
                equipment_id);
}

static void update_equipment(struct mg_connection *connection,
                             struct mg_http_message *message, int equipment_id) {
  char name[128], description[256], category[128], condition[64], status[64],
      location[128];
  (void)body_value(message, "name", name, sizeof(name));
  (void)body_value(message, "description", description, sizeof(description));
  (void)body_value(message, "category", category, sizeof(category));
  (void)body_value(message, "condition", condition, sizeof(condition));
  (void)body_value(message, "status", status, sizeof(status));
  (void)body_value(message, "location", location, sizeof(location));
  if (equipment_id <= 0 || !required_values(name, category, condition, status)) {
    operation_reply(connection, 400, "", "valid equipment data is required");
    return;
  }
  operation_reply(connection, database_equipment_update("data/equipment.db",
                   equipment_id, name, description, category, condition, status,
                   location), "equipment updated", "equipment could not be updated");
}

static void create_loan(struct mg_connection *connection,
                        struct mg_http_message *message) {
  char equipment[32], borrower[128], email[160], due_date[32];
  int loan_id = 0;
  (void)body_value(message, "equipment_id", equipment, sizeof(equipment));
  (void)body_value(message, "borrower_name", borrower, sizeof(borrower));
  (void)body_value(message, "borrower_email", email, sizeof(email));
  (void)body_value(message, "due_date", due_date, sizeof(due_date));
  if (atoi(equipment) <= 0 || borrower[0] == '\0' || email[0] == '\0' ||
      due_date[0] == '\0') {
    operation_reply(connection, 400, "", "equipment, borrower name, email, and due date are required");
    return;
  }
  if (database_loan_create("data/equipment.db", atoi(equipment), borrower, email,
                           due_date, &loan_id) != 0) {
    operation_reply(connection, 400, "", "equipment is unavailable or loan data is invalid");
    return;
  }
  mg_http_reply(connection, 201, "Content-Type: application/json\r\n",
                "{\"status\":\"ok\",\"id\":%d,\"message\":\"loan created\"}\n", loan_id);
}

static void create_maintenance(struct mg_connection *connection,
                               struct mg_http_message *message) {
  char equipment[32], description[256], reported_date[32], cost_text[32];
  int maintenance_id = 0;
  (void)body_value(message, "equipment_id", equipment, sizeof(equipment));
  (void)body_value(message, "description", description, sizeof(description));
  (void)body_value(message, "reported_date", reported_date, sizeof(reported_date));
  (void)body_value(message, "cost", cost_text, sizeof(cost_text));
  if (atoi(equipment) <= 0 || description[0] == '\0' || reported_date[0] == '\0') {
    operation_reply(connection, 400, "", "equipment, description, and reported date are required");
    return;
  }
  operation_reply(connection, database_maintenance_create("data/equipment.db",
                   atoi(equipment), description, reported_date, atof(cost_text),
                   &maintenance_id), "maintenance record created",
                   "maintenance record could not be created");
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
  } else if (mg_match(message->uri, mg_str("/api/equipment"), NULL) &&
             mg_match(message->method, mg_str("POST"), NULL)) {
    create_equipment(connection, message);
  } else if (mg_match(message->uri, mg_str("/api/loans"), NULL) &&
             mg_match(message->method, mg_str("POST"), NULL)) {
    create_loan(connection, message);
  } else if (mg_match(message->uri, mg_str("/api/loans/return/#"), NULL) &&
             mg_match(message->method, mg_str("POST"), NULL)) {
    operation_reply(connection, database_loan_return("data/equipment.db",
                     equipment_id_from_uri(message)), "loan returned",
                     "loan could not be returned");
  } else if (mg_match(message->uri, mg_str("/api/loans/return-equipment/#"), NULL) &&
             mg_match(message->method, mg_str("POST"), NULL)) {
    operation_reply(connection, database_loan_return_equipment("data/equipment.db",
                     equipment_id_from_uri(message)), "loan returned",
                     "no active loan found for this equipment");
  } else if (mg_match(message->uri, mg_str("/api/maintenance"), NULL) &&
             mg_match(message->method, mg_str("GET"), NULL)) {
    char json[16384];
    if (database_maintenance_json("data/equipment.db", json, sizeof(json)) != 0) {
      operation_reply(connection, 500, "", "maintenance records unavailable");
    } else {
      mg_http_reply(connection, 200, "Content-Type: application/json\r\n",
                    "%s\n", json);
    }
  } else if (mg_match(message->uri, mg_str("/api/maintenance"), NULL) &&
             mg_match(message->method, mg_str("POST"), NULL)) {
    create_maintenance(connection, message);
  } else if (mg_match(message->uri, mg_str("/api/maintenance/#"), NULL) &&
             mg_match(message->method, mg_str("PUT"), NULL)) {
    char description[256], cost_text[32];
    (void)body_value(message, "description", description, sizeof(description));
    (void)body_value(message, "cost", cost_text, sizeof(cost_text));
    if (description[0] == '\0') {
      operation_reply(connection, 400, "", "maintenance description is required");
    } else {
      operation_reply(connection, database_maintenance_update("data/equipment.db",
                       equipment_id_from_uri(message), description, atof(cost_text)),
                       "maintenance record updated",
                       "maintenance record could not be updated");
    }
  } else if (mg_match(message->uri, mg_str("/api/maintenance/complete/#"), NULL) &&
             mg_match(message->method, mg_str("POST"), NULL)) {
    operation_reply(connection, database_maintenance_complete("data/equipment.db",
                     equipment_id_from_uri(message)), "maintenance completed",
                     "maintenance record could not be completed");
  } else if (mg_match(message->uri, mg_str("/api/equipment/search"), NULL)) {
    send_search(connection, message);
  } else if (mg_match(message->uri, mg_str("/api/equipment"), NULL)) {
    send_equipment(connection, 0);
  } else if (mg_match(message->uri, mg_str("/api/equipment/#"), NULL)) {
    if (mg_match(message->method, mg_str("PUT"), NULL)) {
      update_equipment(connection, message, equipment_id_from_uri(message));
    } else if (mg_match(message->method, mg_str("DELETE"), NULL)) {
      operation_reply(connection, database_equipment_delete("data/equipment.db",
                       equipment_id_from_uri(message)), "equipment deleted",
                       "equipment could not be deleted");
    } else {
      send_equipment(connection, equipment_id_from_uri(message));
    }
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
  } else if (mg_match(message->uri, mg_str("/manage"), NULL)) {
    mg_http_reply(connection, 200, "Content-Type: text/html; charset=utf-8\r\n",
                  "<!doctype html><html><head><meta charset=\"utf-8\">"
                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "<title>Manage Equipment</title>"
                  "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
                  "rel=\"stylesheet\"></head><body><main class=\"container py-4\">"
                  "<h1>Equipment Management</h1><p id=\"message\"></p>"
                  "<h2 class=\"h4\">Add equipment</h2><form id=\"equipment\" class=\"row g-2\">"
                  "<input class=\"form-control\" name=\"name\" placeholder=\"Name\" required>"
                  "<input class=\"form-control\" name=\"category\" placeholder=\"Category\" required>"
                  "<input class=\"form-control\" name=\"description\" placeholder=\"Description\">"
                  "<input class=\"form-control\" name=\"condition\" placeholder=\"Condition (New, Good, Fair, Damaged)\" required>"
                  "<input class=\"form-control\" name=\"status\" value=\"Available\" required>"
                  "<input class=\"form-control\" name=\"location\" placeholder=\"Location\">"
                  "<button class=\"btn btn-primary\" type=\"submit\">Add equipment</button></form>"
                  "<h2 class=\"h4 mt-4\">Borrow equipment</h2><form id=\"loan\" class=\"row g-2\">"
                  "<input class=\"form-control\" name=\"equipment_id\" placeholder=\"Equipment ID\" required>"
                  "<input class=\"form-control\" name=\"borrower_name\" placeholder=\"Borrower name\" required>"
                  "<input class=\"form-control\" name=\"borrower_email\" placeholder=\"Borrower email\" required>"
                  "<input class=\"form-control\" name=\"due_date\" type=\"date\" required>"
                  "<button class=\"btn btn-success\" type=\"submit\">Create loan</button></form>"
                  "<h2 class=\"h4 mt-4\">Record maintenance</h2><form id=\"maintenance\" class=\"row g-2\">"
                  "<input class=\"form-control\" name=\"equipment_id\" placeholder=\"Equipment ID\" required>"
                  "<input class=\"form-control\" name=\"description\" placeholder=\"Description\" required>"
                  "<input class=\"form-control\" name=\"reported_date\" type=\"date\" required>"
                  "<input class=\"form-control\" name=\"cost\" type=\"number\" step=\"0.01\" placeholder=\"Cost\">"
                  "<button class=\"btn btn-warning\" type=\"submit\">Create maintenance record</button></form>"
                  "<h2 class=\"h4 mt-4\">Update or complete records</h2>"
                  "<form id=\"update\" class=\"row g-2\"><input class=\"form-control\" name=\"id\" "
                  "placeholder=\"Equipment ID\" required><input class=\"form-control\" name=\"name\" "
                  "placeholder=\"Name\" required><input class=\"form-control\" name=\"category\" "
                  "placeholder=\"Category\" required><input class=\"form-control\" name=\"description\" "
                  "placeholder=\"Description\"><input class=\"form-control\" name=\"condition\" "
                  "placeholder=\"Condition\" required><input class=\"form-control\" name=\"status\" "
                  "placeholder=\"Status\" required><input class=\"form-control\" name=\"location\" "
                  "placeholder=\"Location\"><button class=\"btn btn-info\" type=\"submit\">Update equipment</button></form>"
                  "<form id=\"delete\" class=\"row g-2 mt-2\"><input class=\"form-control\" name=\"id\" "
                  "placeholder=\"Equipment ID\" required><button class=\"btn btn-danger\" type=\"submit\">Delete equipment</button></form>"
                  "<form id=\"return\" class=\"row g-2 mt-2\"><input class=\"form-control\" name=\"id\" "
                  "placeholder=\"Equipment ID\" required><button class=\"btn btn-success\" type=\"submit\">Record return</button></form>"
                  "<form id=\"complete\" class=\"row g-2 mt-2\"><input class=\"form-control\" name=\"id\" "
                  "placeholder=\"Maintenance ID\" required><button class=\"btn btn-secondary\" type=\"submit\">Complete maintenance</button></form>"
                  "<form id=\"maintenance-update\" class=\"row g-2 mt-2\"><input class=\"form-control\" name=\"id\" "
                  "placeholder=\"Maintenance ID\" required><input class=\"form-control\" name=\"description\" "
                  "placeholder=\"Updated description\" required><input class=\"form-control\" name=\"cost\" "
                  "type=\"number\" step=\"0.01\" placeholder=\"Updated cost\"><button class=\"btn btn-info\" "
                  "type=\"submit\">Update maintenance</button></form>"
                  "<p class=\"mt-4\"><a href=\"/equipment\">Catalog</a> | <a href=\"/\">Home</a></p>"
                  "<script>const message=document.getElementById('message');"
                  "async function submitForm(event,id,url){event.preventDefault();const form=document.getElementById(id);"
                  "const response=await fetch(url,{method:'POST',body:new URLSearchParams(new FormData(form))});"
                  "const data=await response.json();message.textContent=data.message||'Request complete';"
                  "message.className=response.ok?'text-success':'text-danger';if(response.ok)form.reset();}"
                  "async function recordForm(event,id,url,method){event.preventDefault();const form=document.getElementById(id);"
                  "const value=new FormData(form), target=url.replace('{id}',value.get('id'));value.delete('id');"
                  "const response=await fetch(target,{method:method,body:new URLSearchParams(value)});"
                  "const data=await response.json();message.textContent=data.message||'Request complete';"
                  "message.className=response.ok?'text-success':'text-danger';if(response.ok)form.reset();}"
                  "document.getElementById('equipment').onsubmit=e=>submitForm(e,'equipment','/api/equipment');"
                  "document.getElementById('loan').onsubmit=e=>submitForm(e,'loan','/api/loans');"
                  "document.getElementById('maintenance').onsubmit=e=>submitForm(e,'maintenance','/api/maintenance');"
                  "document.getElementById('update').onsubmit=e=>recordForm(e,'update','/api/equipment/{id}','PUT');"
                  "document.getElementById('delete').onsubmit=e=>recordForm(e,'delete','/api/equipment/{id}','DELETE');"
                  "document.getElementById('return').onsubmit=e=>recordForm(e,'return','/api/loans/return-equipment/{id}','POST');"
                  "document.getElementById('complete').onsubmit=e=>recordForm(e,'complete','/api/maintenance/complete/{id}','POST');"
                  "document.getElementById('maintenance-update').onsubmit=e=>recordForm(e,'maintenance-update','/api/maintenance/{id}','PUT');"
                  "</script></main></body></html>");
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
                  "<a class=\"btn btn-primary\" href=\"/equipment\">Browse equipment</a> "
                  "<a class=\"btn btn-secondary\" href=\"/manage\">Manage equipment</a>"
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
