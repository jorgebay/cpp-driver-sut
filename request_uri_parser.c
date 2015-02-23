
#line 1 "request_uri_parser.rl"
#include "request_uri_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#line 11 "request_uri_parser.c"
static const char _request_uri_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 2, 1, 8, 2, 1, 
	9, 2, 1, 10, 2, 1, 11
};

static const char _request_uri_key_offsets[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 
	8, 9, 10, 11, 12, 13, 14, 15, 
	16, 17, 18, 19, 20, 21, 22, 23, 
	24, 25, 26, 27, 28, 29, 30, 31, 
	32, 33, 35, 36, 37, 38, 39, 40, 
	41, 42, 43, 44, 45, 46, 47, 48, 
	49, 50, 51, 52, 53, 54, 55, 56, 
	57, 58, 60, 61, 65, 66, 67, 70, 
	73, 74, 77, 78, 81, 84, 85, 88
};

static const char _request_uri_trans_keys[] = {
	97, 115, 115, 97, 110, 100, 114, 97, 
	114, 101, 112, 97, 114, 101, 100, 45, 
	115, 116, 97, 116, 101, 109, 101, 110, 
	116, 115, 47, 117, 115, 101, 114, 115, 
	47, 48, 57, 105, 109, 112, 108, 101, 
	45, 115, 116, 97, 116, 101, 109, 101, 
	110, 116, 115, 47, 117, 115, 101, 114, 
	115, 47, 48, 57, 47, 47, 99, 112, 
	115, 47, 47, 47, 48, 57, 47, 48, 
	57, 47, 47, 48, 57, 47, 47, 48, 
	57, 47, 48, 57, 47, 47, 48, 57, 
	47, 0
};

static const char _request_uri_single_lengths[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 0, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 0, 1, 4, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1
};

static const char _request_uri_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 1, 1, 
	0, 1, 0, 1, 1, 0, 1, 0
};

static const unsigned char _request_uri_index_offsets[] = {
	0, 2, 4, 6, 8, 10, 12, 14, 
	16, 18, 20, 22, 24, 26, 28, 30, 
	32, 34, 36, 38, 40, 42, 44, 46, 
	48, 50, 52, 54, 56, 58, 60, 62, 
	64, 66, 68, 70, 72, 74, 76, 78, 
	80, 82, 84, 86, 88, 90, 92, 94, 
	96, 98, 100, 102, 104, 106, 108, 110, 
	112, 114, 116, 118, 123, 125, 127, 130, 
	133, 135, 138, 140, 143, 146, 148, 151
};

static const char _request_uri_trans_targs[] = {
	1, 58, 2, 58, 3, 58, 4, 58, 
	5, 58, 6, 58, 7, 58, 61, 58, 
	9, 58, 10, 58, 11, 58, 12, 58, 
	13, 58, 14, 58, 15, 58, 16, 58, 
	17, 58, 18, 58, 19, 58, 20, 58, 
	21, 58, 22, 58, 23, 58, 24, 58, 
	25, 58, 26, 58, 27, 58, 28, 58, 
	29, 58, 30, 58, 31, 58, 32, 58, 
	33, 58, 62, 58, 35, 58, 36, 58, 
	37, 58, 38, 58, 39, 58, 40, 58, 
	41, 58, 42, 58, 43, 58, 44, 58, 
	45, 58, 46, 58, 47, 58, 48, 58, 
	49, 58, 50, 58, 51, 58, 52, 58, 
	53, 58, 54, 58, 55, 58, 56, 58, 
	57, 58, 67, 58, 59, 58, 60, 0, 
	8, 34, 58, 60, 58, 61, 58, 63, 
	62, 58, 64, 65, 58, 64, 58, 66, 
	65, 58, 66, 58, 68, 67, 58, 69, 
	70, 58, 69, 58, 71, 70, 58, 71, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	58, 58, 58, 58, 58, 58, 58, 58, 
	0
};

static const char _request_uri_trans_actions[] = {
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 1, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 0, 25, 0, 25, 0, 25, 
	0, 25, 1, 25, 9, 11, 0, 0, 
	0, 0, 13, 0, 13, 0, 15, 3, 
	0, 30, 0, 1, 19, 0, 19, 3, 
	0, 36, 0, 23, 3, 0, 27, 0, 
	1, 17, 0, 17, 3, 0, 33, 0, 
	21, 25, 25, 25, 25, 25, 25, 25, 
	25, 25, 25, 25, 25, 25, 25, 25, 
	25, 25, 25, 25, 25, 25, 25, 25, 
	25, 25, 25, 25, 25, 25, 25, 25, 
	25, 25, 25, 25, 25, 25, 25, 25, 
	25, 25, 25, 25, 25, 25, 25, 25, 
	25, 25, 25, 25, 25, 25, 25, 25, 
	25, 25, 25, 13, 13, 15, 30, 19, 
	19, 36, 23, 27, 17, 17, 33, 21, 
	0
};

static const char _request_uri_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 5, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _request_uri_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 7, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char _request_uri_eof_trans[] = {
	211, 211, 211, 211, 211, 211, 211, 211, 
	211, 211, 211, 211, 211, 211, 211, 211, 
	211, 211, 211, 211, 211, 211, 211, 211, 
	211, 211, 211, 211, 211, 211, 211, 211, 
	211, 211, 211, 211, 211, 211, 211, 211, 
	211, 211, 211, 211, 211, 211, 211, 211, 
	211, 211, 211, 211, 211, 211, 211, 211, 
	211, 211, 0, 213, 213, 214, 215, 217, 
	217, 218, 219, 220, 222, 222, 223, 224
};

static const int request_uri_start = 58;
static const int request_uri_first_final = 58;
static const int request_uri_error = -1;

static const int request_uri_en_main = 58;


#line 10 "request_uri_parser.rl"


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

  
#line 207 "request_uri_parser.c"
	{
	cs = request_uri_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 215 "request_uri_parser.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _request_uri_actions + _request_uri_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 3:
#line 1 "NONE"
	{ts = p;}
	break;
#line 234 "request_uri_parser.c"
		}
	}

	_keys = _request_uri_trans_keys + _request_uri_key_offsets[cs];
	_trans = _request_uri_index_offsets[cs];

	_klen = _request_uri_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _request_uri_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
_eof_trans:
	cs = _request_uri_trans_targs[_trans];

	if ( _request_uri_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _request_uri_actions + _request_uri_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 26 "request_uri_parser.rl"
	{
      sections[current_section].start = p;
    }
	break;
	case 1:
#line 30 "request_uri_parser.rl"
	{
      sections[current_section].end = p;
      //printf("%.*s\n", (int)(sections[current_section].end - sections[current_section].start), sections[current_section].start);
      current_section++;
    }
	break;
	case 4:
#line 1 "NONE"
	{te = p+1;}
	break;
	case 5:
#line 52 "request_uri_parser.rl"
	{te = p+1;{ result = REQUEST_URI_UNKNOWN; }}
	break;
	case 6:
#line 46 "request_uri_parser.rl"
	{te = p;p--;{ result = REQUEST_URI_ROOT; }}
	break;
	case 7:
#line 47 "request_uri_parser.rl"
	{te = p;p--;{ result = REQUEST_URI_CASSANDRA; }}
	break;
	case 8:
#line 48 "request_uri_parser.rl"
	{te = p;p--;{ result = REQUEST_URI_SIMPLE_USER_SINGLE; }}
	break;
	case 9:
#line 49 "request_uri_parser.rl"
	{te = p;p--;{ result = REQUEST_URI_PREPARED_USER_SINGLE; }}
	break;
	case 10:
#line 50 "request_uri_parser.rl"
	{te = p;p--;{ result = REQUEST_URI_SIMPLE_USER_MULTIPLE; }}
	break;
	case 11:
#line 51 "request_uri_parser.rl"
	{te = p;p--;{ result = REQUEST_URI_PREPARED_USER_MULTIPLE; }}
	break;
	case 12:
#line 46 "request_uri_parser.rl"
	{{p = ((te))-1;}{ result = REQUEST_URI_ROOT; }}
	break;
#line 349 "request_uri_parser.c"
		}
	}

_again:
	_acts = _request_uri_actions + _request_uri_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 2:
#line 1 "NONE"
	{ts = 0;}
	break;
#line 362 "request_uri_parser.c"
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _request_uri_eof_trans[cs] > 0 ) {
		_trans = _request_uri_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	}

#line 57 "request_uri_parser.rl"


  return result;
}
