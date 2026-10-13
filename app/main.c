#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signal_number) {
    (void)signal_number;
    keep_running = 0;
}

static void send_response(int client, const char *status, const char *content_type,
                          const char *body) {
    char response[BUFFER_SIZE];
    int length = snprintf(response, sizeof(response),
                          "HTTP/1.1 %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n\r\n%s",
                          status, content_type, strlen(body), body);
    if (length > 0) {
        (void)send(client, response, (size_t)length, 0);
    }
}

int main(void) {
    const char *port_value = getenv("PORT");
    int port = port_value != NULL ? atoi(port_value) : DEFAULT_PORT;
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int reuse_address = 1;
    (void)setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                     sizeof(reuse_address));

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons((uint16_t)port),
    };
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(server, 16) < 0) {
        perror("bind/listen");
        close(server);
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    printf("Equipment Lending Library running at http://localhost:%d/\n", port);
    fflush(stdout);

    while (keep_running) {
        int client = accept(server, NULL, NULL);
        if (client < 0) {
            if (keep_running) {
                perror("accept");
            }
            continue;
        }

        char request[BUFFER_SIZE] = {0};
        (void)recv(client, request, sizeof(request) - 1, 0);
        if (strncmp(request, "GET /api/health", 15) == 0) {
            send_response(client, "200 OK", "application/json",
                          "{\"status\":\"ok\"}");
        } else if (strncmp(request, "GET /", 5) == 0) {
            send_response(client, "200 OK", "text/html; charset=utf-8",
                          "<!doctype html><html lang=\"en\"><head>"
                          "<meta charset=\"utf-8\"><meta name=\"viewport\" "
                          "content=\"width=device-width, initial-scale=1\">"
                          "<title>Equipment Lending Library</title>"
                          "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
                          "rel=\"stylesheet\"></head><body><main class=\"container py-5\">"
                          "<h1>Equipment Lending Library</h1>"
                          "<p class=\"lead\">Hello World! The EC2 deployment is working.</p>"
                          "<p>Catalog, lending, and maintenance features will be added next.</p>"
                          "</main></body></html>");
        } else {
            send_response(client, "404 Not Found", "text/plain", "Not found");
        }
        close(client);
    }

    close(server);
    return EXIT_SUCCESS;
}
