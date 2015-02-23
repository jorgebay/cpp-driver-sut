#include "fastercgi.h"

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#define FCGI_END_REQUEST_LENGTH 8

#define FCGI_RECORD_STATE_HEADER_DONE  1
#define FCGI_RECORD_STATE_CONTENT_DONE 2
#define FCGI_RECORD_STATE_RECORD_DONE  3

#define FCGI_STATE_BEGIN  1
#define FCGI_STATE_ABORT  2
#define FCGI_STATE_PARAMS 3
#define FCGI_STATE_STDIN  4
#define FCGI_STATE_WRITE  5
#define FCGI_STATE_NOTIFY 6

#define FCGI_WRITE_STATE_DATA_SENT 1
#define FCGI_WRITE_STATE_EOS_SENT  2

/*****************************************************************************/

static void fcgi__on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
static void fcgi__on_async(uv_async_t* async);
static void fcgi__on_connection(uv_stream_t* stream, int status);
static void fcgi__on_close(uv_handle_t* handle);
static void fcgi__on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
static void fcgi__on_signal(uv_signal_t* sig, int signum);
static void fcgi__on_write(uv_write_t* write, int status);
static void fcgi__on_write_end(uv_write_t* write, int status);

static void fcgi__buffer_init(fcgi_buffer_t* buf);
static void fcgi__connection_close(fcgi_connection_t* conn);
static void fcgi__connection_init(fcgi_connection_t* conn, fcgi_server_t* serv);
static void fcgi__connection_reset(fcgi_connection_t* conn);
static fcgi_connection_t* fcgi__server_get_connection(fcgi_server_t* serv);
static void fcgi__write_req_init(fcgi_write_req_t* req, fcgi_connection_t* conn);
static void fcgi__write_req_reset(fcgi_write_req_t* req);

/*****************************************************************************/

void fcgi__on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  fcgi_connection_t* conn = (fcgi_connection_t*)handle->data;
  if (!conn->to_read) {
    conn->to_read = (char*)malloc(suggested_size);
  }
  buf->base = conn->to_read;
  buf->len = suggested_size;
}

void fcgi__on_async(uv_async_t* async) {
  fcgi_connection_t* conn = (fcgi_connection_t*)async->data;
  conn->serv->handler_cb(conn, FCGI_STATE_NOTIFY);
}

void fcgi__on_connection(uv_stream_t* stream, int status) {
  if (status < 0) {
    fprintf(stderr, "Conenction error %s\n", uv_strerror(status));
    return;
  }

  fcgi_connection_t* conn = fcgi__server_get_connection((fcgi_server_t*)stream->data);

  if (!conn) {
    fprintf(stderr, "No more connections available\n");
    abort();
    return;
  }

  uv_pipe_init(stream->loop, &conn->pipe, 0);

  if (uv_accept(stream, (uv_stream_t*)&conn->pipe) == 0) {
    conn->is_closed = false;
    conn->in_use = true;
    uv_read_start((uv_stream_t*)&conn->pipe, fcgi__on_alloc, fcgi__on_read);
  } else {
    uv_close((uv_handle_t*)&conn->pipe, fcgi__on_close);
  }
}

void fcgi__on_close(uv_handle_t* handle) {
  fcgi_connection_t* conn = (fcgi_connection_t*)handle->data;
  conn->is_closed = true;
  if (!conn->in_use && !conn->in_free_list) {
    fcgi__connection_reset(conn);
  }
}

void fcgi__on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  fcgi_connection_t* conn = (fcgi_connection_t*)stream->data;

  if (nread < 0) {
    fcgi__connection_close(conn);
    return;
  }

  size_t remaining = nread;
  const char* pos = buf->base;

  while (remaining > 0) {
    if (conn->record_length < FCGI_RECORD_HEADER_LENGTH) {
      size_t to_copy = FCGI_RECORD_HEADER_LENGTH - conn->record_length;
      if (to_copy > remaining) to_copy = remaining;
      memcpy(conn->header_buf + conn->record_length, pos, to_copy);
      conn->record_length += to_copy;
      pos += to_copy;
      remaining -= to_copy;
    }

    if(conn->record_state == 0 && conn->record_length >= FCGI_RECORD_HEADER_LENGTH) {
      conn->version = (uint8_t)conn->header_buf[0];
      conn->type = (uint8_t)conn->header_buf[1];
      conn->request_id = ((uint8_t)conn->header_buf[2] << 8) + (uint8_t)conn->header_buf[3];
      conn->content_length = ((uint8_t)conn->header_buf[4] << 8) + (uint8_t)conn->header_buf[5];
      conn->padding_length = (uint8_t)conn->header_buf[6];

      conn->record_state = FCGI_RECORD_STATE_HEADER_DONE;
    }

    if (conn->record_state == FCGI_RECORD_STATE_HEADER_DONE) {
      size_t total_length = FCGI_RECORD_HEADER_LENGTH + conn->content_length;

      if (conn->record_length < total_length) {
        size_t to_copy = conn->content_length;
        if (to_copy > remaining) to_copy = remaining;

        fcgi_buffer_append(&conn->incoming_buf, pos, to_copy);

        conn->record_length += to_copy;
        pos += to_copy;
        remaining -= to_copy;
      }

      if (conn->record_length == total_length) {
        conn->record_state = FCGI_RECORD_STATE_CONTENT_DONE;
      }
    }

    if (conn->record_state == FCGI_RECORD_STATE_CONTENT_DONE) {
      size_t total_length = FCGI_RECORD_HEADER_LENGTH + conn->content_length + conn->padding_length;

      if (conn->record_length < total_length) {
        size_t to_copy = conn->padding_length;
        if (to_copy > remaining) to_copy = remaining;
        conn->record_length += to_copy;
        pos += to_copy;
        remaining -= to_copy;
      }

      if (conn->record_length == total_length) {
        conn->record_state = FCGI_RECORD_STATE_RECORD_DONE;
      }
    }

    if (conn->record_state == FCGI_RECORD_STATE_RECORD_DONE) {
      const char* content = conn->incoming_buf.data;

      switch (conn->type) {
        case FCGI_BEGIN_REQUEST:
          //printf("begin request\n");
          conn->role = (content[0] << 8) + content[1];
          conn->flags = content[2];
          conn->serv->handler_cb(conn, FCGI_STATE_BEGIN);
          fcgi_buffer_reset(&conn->incoming_buf);
          break;

        case FCGI_ABORT_REQUEST:
          //printf("abort request\n");
          conn->serv->handler_cb(conn, FCGI_STATE_ABORT);
          fcgi__connection_close(conn);
          break;

        case FCGI_PARAMS:
          //printf("params\n");
          if (conn->content_length == 0) {
            //printf("params done\n");
            conn->serv->handler_cb(conn, FCGI_STATE_PARAMS);
            fcgi_buffer_reset(&conn->incoming_buf);
          }
          break;

        case FCGI_STDIN:
          //printf("stdin\n");
          if (conn->content_length == 0) {
            //printf("stdin done\n");
            conn->serv->handler_cb(conn, FCGI_STATE_STDIN);
            fcgi_buffer_reset(&conn->incoming_buf);
          }
          break;

        default:
          fprintf(stderr, "Unhandled record type %d\n", (int)conn->type);
          fcgi_buffer_reset(&conn->incoming_buf);
          break;
      }

      conn->record_state = 0;
      conn->record_length = 0;
    }
  }
}

void fcgi__on_signal(uv_signal_t* sig, int signum) { 
  /* Ignore SIGPIPE */
}

void fcgi__on_write(uv_write_t* write, int status) {
  fcgi_write_req_t* req = (fcgi_write_req_t*)write->data;
  fcgi_write_request_send(req);
}

void fcgi__on_write_end(uv_write_t* write, int status) {
  fcgi_write_req_t* req = (fcgi_write_req_t*)write->data;
  fcgi__write_req_reset(req);
  req->conn->serv->handler_cb(req->conn, FCGI_STATE_WRITE);
}

/*****************************************************************************/


void fcgi__buffer_init(fcgi_buffer_t* buf) {
  buf->capacity = 0;
  buf->length = 0;
  buf->position = 0;
  buf->data = NULL;
}

void fcgi__connection_close(fcgi_connection_t* conn) {
  if (!uv_is_closing((uv_handle_t*)&conn->pipe)) {
    uv_close((uv_handle_t*)&conn->pipe, fcgi__on_close);
  }
}

void fcgi__connection_init(fcgi_connection_t* conn, fcgi_server_t* serv) {
  conn->serv = serv;

  conn->next_in_list = conn->serv->free_list;
  conn->serv->free_list = conn;

  conn->in_free_list = true;
  conn->in_use = false;
  conn->is_closed = true;

  conn->data = NULL;

  fcgi__buffer_init(&conn->incoming_buf);

  conn->to_read = NULL;

  conn->record_state = 0;
  conn->record_length = 0;

  conn->version = 0;
  conn->type = 0;
  conn->request_id = 0;
  conn->content_length = 0;
  conn->padding_length = 0;

  conn->role = 0;
  conn->flags = 0;

  conn->app_status = 200;
  conn->proto_status = FCGI_REQUEST_COMPLETE;

  conn->free_list = NULL;

  conn->pipe.data = conn;
  uv_pipe_init(&serv->loop, &conn->pipe, 0);
  conn->async.data = conn;
  uv_async_init(&serv->loop, &conn->async, fcgi__on_async);
}

void fcgi__connection_reset(fcgi_connection_t* conn) {
  conn->next_in_list = conn->serv->free_list;
  conn->serv->free_list = conn;

  conn->in_free_list = true;

  fcgi_buffer_reset(&conn->incoming_buf);

  conn->record_state = 0;
  conn->record_length = 0;

  conn->version = 1;
  conn->type = 0;
  conn->request_id = 0;
  conn->content_length = 0;
  conn->padding_length = 0;

  conn->role = 0;
  conn->flags = 0;

  conn->app_status = 200;
  conn->proto_status = FCGI_REQUEST_COMPLETE;
}

fcgi_connection_t* fcgi__server_get_connection(fcgi_server_t* serv) {
  fcgi_connection_t* conn = serv->free_list;
  if (conn) {
    serv->free_list = conn->next_in_list;
    conn->in_free_list = false;
  }
  return conn;
}

void fcgi__write_req_init(fcgi_write_req_t* req, fcgi_connection_t* conn) {
  req->conn = conn;
  req->next_in_list = req->conn->free_list;
  req->conn->free_list = req;

  req->type = 0;
  fcgi__buffer_init(&req->outgoing_buf);

  req->req.data = req;
}

void fcgi__write_req_reset(fcgi_write_req_t* req) {
  req->next_in_list = req->conn->free_list;
  req->conn->free_list = req;

  req->type = 0;
  fcgi_buffer_reset(&req->outgoing_buf);
}

/*****************************************************************************/

void fcgi_buffer_reset(fcgi_buffer_t* buf) {
  buf->length = 0;
  buf->position = 0;
}

void fcgi_buffer_append(fcgi_buffer_t* buf, const char* data, size_t length) {
  size_t capacity = buf->length + length;
  if (capacity > buf->capacity) {
    capacity = capacity < 4096 ? 2 * capacity : capacity + 4096;
    buf->data = (char*)realloc(buf->data, capacity);
    buf->capacity = capacity;
  }
  memcpy(buf->data + buf->length, data, length);
  buf->length += length;
}

void fcgi_params_init(fcgi_params_t* params, fcgi_buffer_t* buf) {
  params->data = buf->data;
  params->length = buf->length;
  params->position = 0;
  params->name = NULL;
  params->name_length = 0;
  params->value = NULL;
  params->value_length = 0;
}

bool fcgi_params_next(fcgi_params_t* params) {
  const char* pos;

  params->position += (params->name_length + params->value_length);
  if (params->position == params->length) {
    return false;
  }

  params->name = NULL;
  params->name_length = 0;
  params->value = NULL;
  params->value_length = 0;

  pos = params->data + params->position;

  if (pos[0] >> 7) {
    assert(params->length - params->position >= 4);
    params->name_length = (((uint8_t)pos[0] & 0x7F) << 24) + ((uint8_t)pos[1] << 16) + ((uint8_t)pos[2] << 8) + (uint8_t)pos[3];
    params->position += 4;
  } else {
    assert(params->length - params->position >= 1);
    params->name_length = pos[0];
    params->position += 1;
  }

  pos = params->data + params->position;

  if (pos[0] >> 7) {
    assert(params->length - params->position >= 4);
    params->value_length = (((uint8_t)pos[0] & 0x7F) << 24) + ((uint8_t)pos[1] << 16) + ((uint8_t)pos[2] << 8) + (uint8_t)pos[3];
    params->position += 4;
  } else {
    assert(params->length - params->position >= 1);
    params->value_length = pos[0];
    params->position += 1;
  }

  assert(params->length - params->position >= params->name_length + params->value_length);

  params->name = params->data + params->position;
  params->value = params->data + params->position + params->name_length;

  return true;
}


fcgi_write_req_t* fcgi_connection_get_write_request(fcgi_connection_t* conn, int type) {
  fcgi_write_req_t* req = conn->free_list;
  if (req) {
    conn->free_list = req->next_in_list;
  } else {
    req = (fcgi_write_req_t*)malloc(sizeof(fcgi_write_req_t));
    fcgi__write_req_init(req, conn);
  }
  req->type = type;
  return req;
}

void fcgi_write_request_send(fcgi_write_req_t* req) {
  char* to_write = req->to_write;
  fcgi_buffer_t* buf = &req->outgoing_buf;

  if (req->conn->is_closed) {
    if (!req->conn->in_free_list) {
      fcgi__connection_reset(req->conn);
    }
    fcgi__write_req_reset(req);
    return;
  }

  size_t remaining = buf->length - buf->position;
  if (remaining > 0) {
    size_t to_copy = FCGI_MAX_RECORD_CONTENT_LENGTH;
   if (remaining < to_copy) to_copy = remaining;
    to_write[0] = req->conn->version;
    to_write[1] = req->type;
    to_write[2] = (req->conn->request_id >> 8) & 0xFF;
    to_write[3] = req->conn->request_id & 0x00FF;
    to_write[4] = (to_copy >> 8) & 0xFF;
    to_write[5] = to_copy & 0x00FF;
    to_write[6] = 0;

    memcpy(to_write + FCGI_RECORD_HEADER_LENGTH, buf->data + buf->position, to_copy);
    buf->position += to_copy;

    uv_buf_t uvbuf = uv_buf_init(to_write, FCGI_RECORD_HEADER_LENGTH + to_copy);
    int rc = uv_write(&req->req, (uv_stream_t*)&req->conn->pipe, &uvbuf, 1, fcgi__on_write);
    if (rc != 0) {
      fprintf(stderr, "Write error %s\n", uv_strerror(rc));
      fcgi__write_req_reset(req);
    }
  } else if(req->type == FCGI_STDOUT || req->type == FCGI_STDOUT) {
    to_write[0] = req->conn->version;
    to_write[1] = req->type;
    to_write[2] = (req->conn->request_id >> 8) & 0xFF;
    to_write[3] = req->conn->request_id & 0x00FF;
    to_write[4] = 0;
    to_write[5] = 0;
    to_write[6] = 0;

    uv_buf_t uvbuf = uv_buf_init(to_write, FCGI_RECORD_HEADER_LENGTH);
    int rc = uv_write(&req->req, (uv_stream_t*)&req->conn->pipe, &uvbuf, 1, fcgi__on_write_end);
    if (rc != 0) {
      fprintf(stderr, "Write error %s\n", uv_strerror(rc));
      fcgi__write_req_reset(req);
    }
  } else if (req->type == FCGI_END_REQUEST) {
    fcgi__write_req_reset(req);
    if ((req->conn->flags & FCGI_KEEP_CONN) == 0) {
      fcgi__connection_close(req->conn);
    } 
  }
}

void fcgi_connection_notify(fcgi_connection_t* conn) {
  uv_async_send(&conn->async);
}

void fcgi_connection_end(fcgi_connection_t* conn) {
  char to_write[FCGI_END_REQUEST_LENGTH];

  conn->in_use = false;

  fcgi_write_req_t* req = fcgi_connection_get_write_request(conn, FCGI_END_REQUEST);

  to_write[0] = (conn->app_status >> 24) & 0x000000FF;
  to_write[1] = (conn->app_status >> 16) & 0x000000FF;
  to_write[2] = (conn->app_status >> 8) & 0x000000FF;
  to_write[3] = conn->app_status & 0x00000FF;
  to_write[4] = conn->proto_status;

  fcgi_buffer_append(&req->outgoing_buf, to_write, FCGI_END_REQUEST_LENGTH);
  fcgi_write_request_send(req);
}

int fcgi_server_init(fcgi_server_t* serv) {
  int i;
  int rc = uv_loop_init(&serv->loop);
  if (rc != 0) return rc;

  serv->free_list = NULL;
  serv->pipe.data = serv;
  uv_pipe_init(&serv->loop, &serv->pipe, 0);

  uv_signal_init(&serv->loop, &serv->sig);
  uv_signal_start(&serv->sig, fcgi__on_signal, SIGPIPE);

  for (i = 0; i < FCGI_MAX_CONNECTIONS; ++i) {
    fcgi_connection_t* conn = &serv->conns[i];
    fcgi__connection_init(conn, serv);
  }

  return 0;
}

int fcgi_server_start(fcgi_server_t* serv, const char* path, fcgi_handler_cb handler_cb) {
  serv->handler_cb = handler_cb;

  uv_pipe_init(&serv->loop, &serv->pipe, 0);

  int rc;
  if ((rc = uv_pipe_bind(&serv->pipe, path)) != 0) {
    fprintf(stderr, "Bind error %s\n", uv_strerror(rc));
    return 1;
  }

  /* Not secure at all */
  chmod(path, 0666);

  if((rc = uv_listen((uv_stream_t*)&serv->pipe, 128, fcgi__on_connection)) != 0) {
    fprintf(stderr, "Listen error %s\n", uv_strerror(rc));
    return 1;
  }

  uv_run(&serv->loop, UV_RUN_DEFAULT);
}
