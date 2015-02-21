#ifndef FASTERCGI_H
#define FASTERCGI_H

#include <uv.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define FCGI_MAX_CONNECTIONS 1024
#define FCGI_RECORD_HEADER_LENGTH 8
#define FCGI_MAX_RECORD_CONTENT_LENGTH (64 * 1024 - 1)

#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3

#define FCGI_KEEP_CONN  1

#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN    1
#define FCGI_OVERLOADED       2
#define FCGI_UNKNOWN_ROLE     3

#define FCGI_STATE_BEGIN  1
#define FCGI_STATE_ABORT  2
#define FCGI_STATE_PARAMS 3
#define FCGI_STATE_STDIN  4
#define FCGI_STATE_WRITE  5
#define FCGI_STATE_NOTIFY 6
#define FCGI_STATE_END    7

struct fcgi_server_s;

typedef struct fcgi_buffer_s {
  size_t capacity;
  size_t length;
  size_t position;
  char* data;
} fcgi_buffer_t;

typedef struct fcgi_params_s {
  const char* data;
  size_t length;
  size_t position;
  const char* name;
  uint32_t name_length;
  const char* value;
  uint32_t value_length;
} fcgi_params_t;

typedef struct fcgi_write_req_s {
  struct fcgi_connection_s* conn;
  struct fcgi_write_req_s* next_in_list;

  int type;
  fcgi_buffer_t outgoing_buf;
  char to_write[FCGI_RECORD_HEADER_LENGTH + FCGI_MAX_RECORD_CONTENT_LENGTH];

  uv_write_t req;
} fcgi_write_req_t;

typedef struct fcgi_connection_s {
  struct fcgi_server_s* serv;
  struct fcgi_connection_s* next_in_list;

  uv_pipe_t pipe;
  uv_async_t async;

  void* data;

  fcgi_buffer_t incoming_buf;
  char* to_read;

  int record_state;
  size_t record_length;

  char header_buf[FCGI_RECORD_HEADER_LENGTH];
  uint8_t version;
  uint8_t type;
  uint16_t request_id;
  uint16_t content_length;
  uint8_t padding_length;

  uint16_t role;
  uint8_t flags;

  uint32_t app_status;
  uint16_t proto_status;

  fcgi_write_req_t* free_list;
} fcgi_connection_t;

typedef void(*fcgi_handler_cb)(fcgi_connection_t* conn, int type);

typedef struct fcgi_server_s {
  uv_loop_t loop;
  uv_signal_t sig;
  uv_pipe_t pipe;

  fcgi_handler_cb handler_cb;
  void* data;

  fcgi_connection_t conns[FCGI_MAX_CONNECTIONS];
  fcgi_connection_t* free_list;
} fcgi_server_t;

void fcgi_buffer_reset(fcgi_buffer_t* buf);
void fcgi_buffer_append(fcgi_buffer_t* buf, const char* data, size_t length);

void fcgi_params_init(fcgi_params_t* params, fcgi_buffer_t* buf);
bool fcgi_params_next(fcgi_params_t* params);

fcgi_write_req_t* fcgi_connection_get_write_request(fcgi_connection_t* conn, int type);
void fcgi_connection_notify(fcgi_connection_t* conn);
void fcgi_connection_end(fcgi_connection_t* conn);

void fcgi_write_request_send(fcgi_write_req_t* req);

int fcgi_server_init(fcgi_server_t* serv);
int fcgi_server_start(fcgi_server_t* serv, const char* path, fcgi_handler_cb handler_cb);

#endif
