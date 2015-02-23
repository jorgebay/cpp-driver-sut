#ifndef REQUEST__URI_PARSER_H
#define REQUEST__URI_PARSER_H

typedef struct uri_section_s {
  const char* start;
  const char* end;
} request_uri_section_t;

enum {
  REQUEST_URI_UNKNOWN,
  REQUEST_URI_ROOT,
  REQUEST_URI_CASSANDRA,
  REQUEST_URI_SIMPLE_USER_SINGLE,
  REQUEST_URI_PREPARED_USER_SINGLE,
  REQUEST_URI_SIMPLE_USER_MULTIPLE,
  REQUEST_URI_PREPARED_USER_MULTIPLE
};

int parse_request_uri(const char* request_uri, request_uri_section_t* sections);

#endif
