#include "fastercgi.h"
#include "request_uri_parser.h"

#include <cassandra.h>
#include <uv.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define min(a, b)             \
  ({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b);   \
   _a < _b ? _a : _b; })

enum {
  UNKNOWN,
  GET,
  POST
};


#define CONTENT_TYPE_TEXT_PLAIN "Content-Type: text/plain\r\n\r\n"

typedef struct request_s {
  int method;
  int type;

  int futures_capacity;
  int futures_length;
  int futures_count;
  CassFuture** futures;

  uv_mutex_t mutex;
} request_t;

#define INITIAL_CAPACITY 256

#define SELECT_QUERY "SELECT username, firstname, lastname, " \
  "password, email, created_date "        \
"FROM videodb.users WHERE username = ?"

#define INSERT_QUERY "INSERT INTO videodb.users " \
  "(username, firstname, lastname, password, created_date) " \
  "VALUES (?, ?, ?, ?, unixTimestampOf(now()))"

const CassPrepared* select_prepared;
const CassPrepared* insert_prepared;

CassError prepare_query(CassSession* session, const char* query, const CassPrepared** prepared) {
  CassError rc = CASS_OK;
  CassFuture* future = NULL;

  future = cass_session_prepare(session, cass_string_init(query));
  cass_future_wait(future);

  rc = cass_future_error_code(future);
  if (rc != CASS_OK) {
    CassString error = cass_future_error_message(future);
    fprintf(stderr, "Query error: %.*s\n",  (int)error.length, error.data);
  } else {
    *prepared = cass_future_get_prepared(future);
  }

  cass_future_free(future);

  return rc;
}

void request_init(request_t* request) {
  request->method = 0;
  request->type = 0;
  request->futures = (CassFuture**)malloc(INITIAL_CAPACITY * sizeof(CassFuture*));
  request->futures_capacity = INITIAL_CAPACITY;
  request->futures_length = 0;
  request->futures_count = 0;
  uv_mutex_init(&request->mutex);
}

void request_reset(request_t* request) {
  request->method = 0;
  request->type = 0;
  request->futures_length = 0;
  request->futures_count = 0;
}

void request_append_future(request_t* request, CassFuture* future) {
  if (request->futures_length + 1 > request->futures_capacity) {
    size_t new_capacity = request->futures_capacity < 4096 
                        ? request->futures_capacity * 2 
                        : request->futures_capacity + 1024;
    request->futures = (CassFuture**)realloc(request->futures, 
                                             new_capacity * sizeof(CassFuture*));
    request->futures_capacity = new_capacity;
  }
  request->futures[request->futures_length++] = future;
}

int stoi(request_uri_section_t* s) {
  char temp[512];
  size_t to_copy = min(sizeof(temp), (size_t)(s->end - s->start));
  memcpy(temp, s->start, to_copy);
  temp[to_copy] = '\0';
  return atoi(temp);
}

void on_future(CassFuture* future, void* data) {
  fcgi_connection_t* conn = (fcgi_connection_t*)data;
  request_t* request = (request_t*)conn->data;
  uv_mutex_lock(&request->mutex);
  if(--request->futures_count <= 0) {
    fcgi_connection_notify(conn);
  }
  uv_mutex_unlock(&request->mutex);
}

void send_status2(fcgi_connection_t* conn, int status, const char* message, size_t message_length) {
  char temp[512];
  conn->app_status = status;
  fcgi_write_req_t* req = fcgi_connection_get_write_request(conn, FCGI_STDOUT);
  snprintf(temp, sizeof(temp), CONTENT_TYPE_TEXT_PLAIN "%.*s", (int)message_length, message);
  fcgi_buffer_append(&req->outgoing_buf, temp, strlen(temp));
  fcgi_write_request_send(req);
}

void send_status(fcgi_connection_t* conn, int status, const char* message) {
  send_status2(conn, status, message, strlen(message));
}

void insert_user(fcgi_connection_t* conn, request_t* request, 
                 const char* id, size_t id_length,
                 bool use_prepared) {
  CassString id_str = cass_string_init2(id, id_length); 
  CassSession* session = (CassSession*)conn->serv->data;
  CassString query = cass_string_init(INSERT_QUERY);
  CassStatement* statement;
  if (use_prepared) {
    statement = cass_prepared_bind(insert_prepared);
  } else {
    statement = cass_statement_new(query, 4);
  }
  cass_statement_bind_string(statement, 0, id_str);
  cass_statement_bind_string(statement, 1, id_str);
  cass_statement_bind_string(statement, 2, id_str);
  cass_statement_bind_string(statement, 3, id_str);
  CassFuture* future = cass_session_execute(session, statement);
  request_append_future(request, future);
  cass_future_set_callback(future, on_future, conn);
  cass_statement_free(statement);
}

void select_user(fcgi_connection_t* conn, request_t* request, 
                 const char* id, size_t id_length, 
                 bool use_prepared) {
  CassString id_str = cass_string_init2(id, id_length); 
  CassSession* session = (CassSession*)conn->serv->data;
  CassString query = cass_string_init(SELECT_QUERY);
  CassStatement* statement;
  if (use_prepared) {
    statement = cass_prepared_bind(select_prepared);
  } else {
    statement = cass_statement_new(query, 1);
  }
  cass_statement_bind_string(statement, 0, id_str);
  CassFuture* future = cass_session_execute(session, statement);
  request_append_future(request, future);
  cass_future_set_callback(future, on_future, conn);
  cass_statement_free(statement);
}

void handle(fcgi_connection_t* conn, int type) {
  char request_method[16];
  char request_uri[512];

  request_method[0] = '\0';
  request_uri[0] = '\0';

  if (type == FCGI_STATE_PARAMS) {
    fcgi_params_t params;
    fcgi_params_init(&params, &conn->incoming_buf);

    while (fcgi_params_next(&params)) {
      if (strncmp(params.name, "REQUEST_URI", params.name_length) == 0) {
        size_t to_copy = min(params.value_length, sizeof(request_uri) - 1);
        memcpy(request_uri, params.value, to_copy);
        request_uri[to_copy] = '\0';
      } else if (strncmp(params.name, "REQUEST_METHOD", params.name_length) == 0) {
        size_t to_copy = min(params.value_length, sizeof(request_method) - 1);
        memcpy(request_method, params.value, to_copy);
        request_method[to_copy] = '\0';
      }
      //printf("%.*s : %.*s\n", (int)params.name_length, params.name,
      //                        (int)params.value_length, params.value);
    }

    //printf("%s %s (conn %p)\n", request_method, request_uri, conn);

    request_t* request;
    if (!conn->data) {
      request = (request_t*)malloc(sizeof(request_t));
      request_init(request);
      conn->data = request;
    } else {
      request = (request_t*)conn->data;
      request_reset(request);
    }


    request_uri_section_t sections[2];
    request->type =  parse_request_uri(request_uri, sections);

    bool prepared = request->type == REQUEST_URI_PREPARED_USER_SINGLE ||
                    request->type == REQUEST_URI_PREPARED_USER_MULTIPLE;
    if (strcmp(request_method, "GET") == 0) {
      request->method = GET;
      switch (request->type) {
        case REQUEST_URI_ROOT: 
          {
            const char* hello = "Content-Type: text/plain\r\n\r\nHello World";
            fcgi_write_req_t* req = fcgi_connection_get_write_request(conn, FCGI_STDOUT);
            fcgi_buffer_append(&req->outgoing_buf, hello, strlen(hello));
            fcgi_write_request_send(req);
          }
          break;
        case REQUEST_URI_CASSANDRA:
          {
            CassSession* session = (CassSession*)conn->serv->data;
            CassString query = cass_string_init("SELECT NOW() FROM system.local");
            CassStatement* statement = cass_statement_new(query, 0);
            CassFuture* future = cass_session_execute(session, statement);
            request_append_future(request, future);
            cass_future_set_callback(future, on_future, conn);
            cass_statement_free(statement);
          }
          break;
        case REQUEST_URI_SIMPLE_USER_SINGLE: /* Fallthrough intended */
        case REQUEST_URI_PREPARED_USER_SINGLE:
          select_user(conn, request, sections[0].start, sections[0].end - sections[0].start, prepared);
          break;
        case REQUEST_URI_SIMPLE_USER_MULTIPLE: /* Fallthrough intended */
        case REQUEST_URI_PREPARED_USER_MULTIPLE:
          {
            char temp[16];
            int id = stoi(&sections[0]);
            int nbusers = stoi(&sections[1]);

            if (nbusers - id <= 0) {
              send_status(conn, 204, "No content");
            } else {
              request->futures_count = nbusers - id;

              int i;
              for (i = id; i < nbusers; ++i) {
                snprintf(temp, 16, "%d", i);
                select_user(conn, request, temp, strlen(temp), prepared);
              }
            }
          }
          break;
        default:
          send_status(conn, 404, "Not found");
          break;
      }
    } else if (strcmp(request_method, "POST") == 0) {
      request->method = POST;
      switch (request->type) {
        case REQUEST_URI_SIMPLE_USER_SINGLE: /* Fallthrough intended */
        case REQUEST_URI_PREPARED_USER_SINGLE:
          insert_user(conn, request, sections[0].start, sections[0].end - sections[0].start, prepared);
          break;

        case REQUEST_URI_SIMPLE_USER_MULTIPLE: /* Fallthrough intended */
        case REQUEST_URI_PREPARED_USER_MULTIPLE:
          {
            char temp[16];
            int id = stoi(&sections[0]);
            int nbusers = stoi(&sections[1]);

            if (nbusers - id <= 0) {
              send_status(conn, 200, "OK");
            } else {
              request->futures_count = nbusers - id;

              int i;
              for (i = id; i < nbusers; ++i) {
                snprintf(temp, 16, "%d", i);
                insert_user(conn, request, temp, strlen(temp), prepared);
              }
            }
          }
          break;
        default:
          send_status(conn, 501, "Not implemented");
          break;
      }
    } else {
      send_status(conn, 501, "Not implemented");
    }
  } else if (type == FCGI_STATE_STDIN) {
  } else if (type == FCGI_STATE_NOTIFY) {
    request_t* request = (request_t*)conn->data;
    switch(request->type) {
      case REQUEST_URI_SIMPLE_USER_SINGLE: /* Fallthrough intended */
      case REQUEST_URI_PREPARED_USER_SINGLE:
        {
          CassFuture* future = request->futures[0];
          if (cass_future_error_code(future) == CASS_OK) {
            if (request->method == GET) {
              const CassResult* result = cass_future_get_result(future);
              if (cass_result_row_count(result) > 0) {
                const CassRow* row = cass_result_first_row(result);
                const CassValue* value = cass_row_get_column_by_name(row, "username");

                CassString username;
                cass_value_get_string(value, &username);
                send_status2(conn, 200, username.data, username.length);
              } else {
                send_status(conn, 404, "Not found");
              }
              cass_result_free(result);
            } else {
              send_status(conn, 201, "Created");
            }
          } else {
            CassString error = cass_future_error_message(future);
            fprintf(stderr, "Query error: %.*s\n",  (int)error.length, error.data);
            send_status2(conn, 500, error.data, error.length);
          }
          cass_future_free(future);
        }
        break;

      case REQUEST_URI_SIMPLE_USER_MULTIPLE: /* Fallthrough intended */
      case REQUEST_URI_PREPARED_USER_MULTIPLE:
        {
          if (request->method == GET) {
            int query_failure_count = 0;
            int i;
            for (i = 0; i < request->futures_length; ++i) {
              CassFuture* future = request->futures[i];
              if (cass_future_error_code(future) != CASS_OK) {
                query_failure_count++;
                //CassString error = cass_future_error_message(future);
                //fprintf(stderr, "Query error: %.*s\n",  (int)error.length, error.data);
              }
            }

            if (query_failure_count == 0) {
              fcgi_write_req_t* req = fcgi_connection_get_write_request(conn, FCGI_STDOUT);
              fcgi_buffer_append(&req->outgoing_buf, CONTENT_TYPE_TEXT_PLAIN, strlen(CONTENT_TYPE_TEXT_PLAIN));
              for (i = 0; i < request->futures_length; ++i) {
                CassFuture* future = request->futures[i];
                const CassResult* result = cass_future_get_result(future);
                if (cass_result_row_count(result) > 0) {
                  const CassRow* row = cass_result_first_row(result);
                  const CassValue* value = cass_row_get_column_by_name(row, "username");
                  CassString username;
                  cass_value_get_string(value, &username);
                  if (i > 0) {
                    fcgi_buffer_append(&req->outgoing_buf, ",", 1);
                  }
                  fcgi_buffer_append(&req->outgoing_buf, username.data, username.length);
                }
                cass_result_free(result);
                cass_future_free(future);
              }
              fcgi_write_request_send(req);
            } else {
              send_status(conn, 500, "Query failures");
            }
            
          } else {
            int query_failure_count = 0;
            int i;
            for (i = 0; i < request->futures_length; ++i) {
              CassFuture* future = request->futures[i];
              if (cass_future_error_code(future) != CASS_OK) {
                query_failure_count++;
                CassString error = cass_future_error_message(future);
                fprintf(stderr, "Query error: %.*s\n",  (int)error.length, error.data);
              }
              cass_future_free(future);
            }

            if (query_failure_count == 0) {
              send_status(conn, 201, "Created");
            } else {
              send_status(conn, 500, "Query failures");
            }
          }
        } 
        break;

      default:
        {
          int i;
          for (i = 0; i < request->futures_length; ++i) {
            CassFuture* future = request->futures[i];
            cass_future_free(future);
          }
          send_status(conn, 200, "OK");
        }
        break;
    }
  } else if (type == FCGI_STATE_WRITE) {
    fcgi_connection_end(conn);
  }
}

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <contact_points> <sock_file>\n", argv[0]);
    return 1;
  }

  CassFuture* connect_future = NULL;
  CassCluster* cluster = cass_cluster_new();
  CassSession* session = cass_session_new();

  cass_cluster_set_contact_points(cluster, argv[1]);
  cass_cluster_set_num_threads_io(cluster, 4);
  cass_cluster_set_queue_size_io(cluster, 10000);
  cass_cluster_set_pending_requests_low_water_mark(cluster, 5000);
  cass_cluster_set_pending_requests_high_water_mark(cluster, 10000);
  cass_cluster_set_core_connections_per_host(cluster, 1);
  cass_cluster_set_max_connections_per_host(cluster, 2);

  connect_future = cass_session_connect(session, cluster);

  if (cass_future_error_code(connect_future) != CASS_OK) {
    return 1;
  }

  if (prepare_query(session, SELECT_QUERY, &select_prepared) != CASS_OK) {
    return 1;
  }

  if (prepare_query(session, INSERT_QUERY, &insert_prepared) != CASS_OK) {
    return 1;
  }

  fcgi_server_t serv;
  serv.data = (void*)session;
  fcgi_server_init(&serv);
  unlink(argv[2]);
  fcgi_server_start(&serv, argv[2], handle);

  return 0;
}

