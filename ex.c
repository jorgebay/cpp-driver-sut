#include "fastercgi.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

void handle(fcgi_connection_t* conn, int type) {
  if (type == FCGI_STATE_PARAMS) {
    fcgi_params_t params;
    fcgi_params_init(&params, &conn->incoming_buf);
    uint32_t status = 404;
    while (fcgi_params_next(&params)) {
      if (strncmp(params.name, "REQUEST_URI", params.name_length) == 0) {
        if (strncmp(params.value, "/", params.value_length) == 0) {
          status = 200;
          const char* hello = "Content-Type: text/plain\r\n\r\nHello, World!";
          fcgi_write_req_t* req = fcgi_connection_get_write_request(conn, FCGI_STDOUT);
          fcgi_buffer_append(&req->outgoing_buf, hello, strlen(hello));
          fcgi_write_request_send(req);
          break;
        }
      }
      printf("%.*s : %.*s\n", (int)params.name_length, params.name,
                              (int)params.value_length, params.value);
    }
    if (status != 200) {
      fcgi_write_req_t* req = fcgi_connection_get_write_request(conn, FCGI_STDOUT);
      const char* not_found = "Content-Type: text/plain\r\n\r\nNot found";
      fcgi_buffer_append(&req->outgoing_buf, not_found, strlen(not_found));
      fcgi_write_request_send(req);
    }
    conn->app_status = status;
  } else if (type == FCGI_STATE_STDIN) {
  } else if (type == FCGI_STATE_WRITE) {
    fcgi_connection_end(conn);
  }
}

int main() {
  fcgi_server_t serv;
  fcgi_server_init(&serv);
  const char* path = "fcgi.0.sock";
  unlink(path);
  fcgi_server_start(&serv, path, handle);
  return 0;
}
