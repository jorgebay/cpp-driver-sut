#include "request_uri_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

%%{
  machine request_uri;
  write data;
}%%

int parse_request_uri(const char* request_uri, request_uri_section_t* sections) {
  int result = REQUEST_URI_UNKNOWN;

  const char* p = request_uri;
  const char* pe = request_uri + strlen(request_uri);
  const char* ts;
  const char* te;
  const char* eof = pe;
  int cs;
  int act;

  int current_section = 0;

  %%{
    action enter_section {
      sections[current_section].start = fpc;
    }

    action finish_section {
      sections[current_section].end = fpc;
      //printf("%.*s\n", (int)(sections[current_section].end - sections[current_section].start), sections[current_section].start);
      current_section++;
    }

    number = ([0-9]+) >enter_section %finish_section;

    root_uri = "/"*;
    cassandra_uri = "/cassandra" "/"*;
    simple_user_single_uri = "/simple-statements/users/" number "/"*;
    prepared_user_single_uri = "/prepared-statements/users/" number "/"*;
    simple_user_multiple_uri = "/simple-statements/users/" number "/" number "/"*;
    prepared_user_multiple_uri = "/prepared-statements/users/" number "/" number "/"*;

    main := |*
      root_uri => { result = REQUEST_URI_ROOT; };
      cassandra_uri => { result = REQUEST_URI_CASSANDRA; };
      simple_user_single_uri => { result = REQUEST_URI_SIMPLE_USER_SINGLE; };
      prepared_user_single_uri => { result = REQUEST_URI_PREPARED_USER_SINGLE; };
      simple_user_multiple_uri => { result = REQUEST_URI_SIMPLE_USER_MULTIPLE; };
      prepared_user_multiple_uri => { result = REQUEST_URI_PREPARED_USER_MULTIPLE; };
      any => { result = REQUEST_URI_UNKNOWN; };
    *|;

    write init;
    write exec;
  }%%

  return result;
}
