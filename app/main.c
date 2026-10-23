#include "database.h"
#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>

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
