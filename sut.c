#include "fastercgi.h"

#include <cassandra.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

void on_future(CassFuture* future, void* data) {
  fcgi_connection_t* conn = (fcgi_connection_t*)data;
  //printf("future returned\n");
  fcgi_connection_notify(conn);
  cass_future_free(future);
}

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
        } else if (strncmp(params.value, "/cassandra", params.value_length) == 0) {
          status = 200;
          CassSession* session = (CassSession*)conn->serv->data;
          CassString query = cass_string_init("SELECT * FROM system.local");
          CassStatement* statement = cass_statement_new(query, 0);
          CassFuture* future = cass_session_execute(session, statement);
          cass_future_set_callback(future, on_future, conn);
          cass_statement_free(statement);
          break;
        }
      }
      //printf("%.*s : %.*s\n", (int)params.name_length, params.name,
      //                        (int)params.value_length, params.value);
    }
    if (status != 200) {
      fcgi_write_req_t* req = fcgi_connection_get_write_request(conn, FCGI_STDOUT);
      const char* not_found = "Content-Type: text/plain\r\n\r\nNot found";
      fcgi_buffer_append(&req->outgoing_buf, not_found, strlen(not_found));
      fcgi_write_request_send(req);
    }
    conn->app_status = status;
  } else if (type == FCGI_STATE_STDIN) {
  } else if (type == FCGI_STATE_NOTIFY) {
    const char* success = "Content-Type: text/plain\r\n\r\nSuccess";
    fcgi_write_req_t* req = fcgi_connection_get_write_request(conn, FCGI_STDOUT);
    fcgi_buffer_append(&req->outgoing_buf, success, strlen(success));
    fcgi_write_request_send(req);
  } else if (type == FCGI_STATE_WRITE) {
    fcgi_connection_end(conn);
  }
}

int main(int argc, char** argv) {
  if (argc < 3) {
    printf("Usage: %s <contact_points> <sock_file>\n", argv[0]);
    return 1;
  }

  CassFuture* connect_future = NULL;
  CassCluster* cluster = cass_cluster_new();
  CassSession* session = cass_session_new();

  cass_cluster_set_contact_points(cluster, argv[1]);
  connect_future = cass_session_connect(session, cluster);

  if (cass_future_error_code(connect_future) != CASS_OK) {
    return 1;
  }

  fcgi_server_t serv;
  serv.data = (void*)session;
  fcgi_server_init(&serv);
  unlink(argv[2]);
  fcgi_server_start(&serv, argv[2], handle);

  return 0;
}

