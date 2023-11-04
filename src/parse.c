/*
Copyright 2023 Solano Felicio

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the “Software”),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pagina.h"

#define ERROR_BUFFER_SIZE 1000
#define REGULAR_BUFFER_SIZE 1000
#define STRING_BUFFER_SIZE 10000

static char error_buffer[ERROR_BUFFER_SIZE];
static char regular_buffer[REGULAR_BUFFER_SIZE];
static char string_buffer[STRING_BUFFER_SIZE];
static long error_pos = -1;
static FILE *input;

static void
init_buffers(void)
{
	strncpy(error_buffer, "", ERROR_BUFFER_SIZE);
	strncpy(regular_buffer, "", REGULAR_BUFFER_SIZE);
	strncpy(string_buffer, "", STRING_BUFFER_SIZE);
}

static void
init_parser(FILE *file)
{
	init_buffers();
	input = file;
}

static char *
str_from_buffer(char *buf, size_t n)
{
	char *str = malloc(sizeof(char)*(n+1));
	strncpy(str, buf, n);
	str[n] = 0;
	return str;
}

static void
errpos(long position, char *message)
{
	size_t n = strlen(message);
	strncpy(error_buffer, message, n);
	assert(n < ERROR_BUFFER_SIZE);
	error_buffer[n] = 0; /* guarantee null-terminated string */
	error_pos = position;
}

static void
err(char *message)
{
	errpos(ftell(input), message);
}

static char
peek(void)
{
	char ch = fgetc(input);
	ungetc(ch, input);
	return ch;
}


/**** lexer ****/

typedef enum token_type
{
	INTEGER,
	FLOAT,
	NAME,
	STRING,
	HEXSTRING,
	LEFT_SQBRAC,
	RIGHT_SQBRAC,
	LEFT_CLBRAC,
	RIGHT_CLBRAC,
	LTLT,
	GTGT,
	PDF_EOF_TOKEN,
	PDF_VERSION_TOKEN,
	COMMENT,
	TRUE_KW,
	FALSE_KW,
	NULL_KW,
	OBJ_KW,
	ENDOBJ_KW,
	STREAM_KW,
	ENDSTREAM_KW,
	TRAILER_KW,
	XREF_KW,
	STARTXREF_KW,
	R_KW,

	/* not actual tokens, just messages from one lexer function to another */
	IO_ERROR_TOKEN,
	LEX_ERROR_TOKEN,
	EOF_TOKEN,
} token_type;

typedef struct token
{
	token_type type;
	size_t length; /* PDF strings are not null-terminated */
	union {
		char *str;
		long intv;
		double floatv;
	} val;
} token;

static void skip_whitespace(void);
static token read_comments(void);
static token read_string(void);
static token read_hex_string(void);
static token read_name(void);
static token read_number(void);
static token read_keyword(void);

static token
read_next(void)
{
	/* assumption: this is called at the beginning of a token */
	token t;
	skip_whitespace();

	char ch = peek();
	switch (ch) {
	case '(':
		return read_string();
	case ')':
		t.type = LEX_ERROR_TOKEN;
		err("Unmatched closing parenthesis");
		return t;
	case '{':
		t.type = LEFT_CLBRAC;
		return t;
	case '}':
		t.type = RIGHT_CLBRAC;
		return t;
	case '<':
		fgetc(input);
		ch = peek();
		if (ch=='<') {
			fgetc(input);
			t.type = LTLT;
			return t;
		}
		return read_hex_string();
	case '>':
		fgetc(input);
		ch = peek();
		if (ch=='>') {
			fgetc(input);
			t.type = GTGT;
			return t;
		}
		t.type = LEX_ERROR_TOKEN;
		err("Unmatched closing angle brackets");
		return t;
	case '%':
		t = read_comments();
		if (t.type != COMMENT)
			return t;
		break;
	case '/':
		return read_name();
	case '[':
		fgetc(input);
		t.type = LEFT_SQBRAC;
		return t;
	case ']':
		fgetc(input);
		t.type = RIGHT_SQBRAC;
		return t;
	case EOF:
		err("EOF reached");
		t.type = EOF_TOKEN;
		return t;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	case '.': case '-': case '+':
		return read_number();
	default:
		return read_keyword();
	}

	return t;
}

static token
peek_token(void)
{
	long pos = ftell(input);
	token t = read_next();
	fseek(input, pos, SEEK_SET);
	return t;
}

static int
is_whitespace(char ch)
{
	/* Null, tab, line feed, form feed, carriage return, space */
	return ch==0 || ch==9 || ch==10 || ch==12 || ch==13 || ch==32;
}

static int is_delimiter(char ch)
{
	return ch=='(' || ch==')' || ch=='<' || ch=='>' || ch=='[' ||
		ch==']' || ch=='{' || ch=='}' || ch=='/' || ch=='%';
}

static int
is_regular(char ch)
{
	return !is_whitespace(ch) && !is_delimiter(ch) && ch != EOF;
}

static int
is_number(char ch)
{
	return '0' <= ch && ch <= '9';
}

static int
is_octal(char ch)
{
	return '0' <= ch && ch <= '7';
}

static int
is_hex(char ch)
{
	return is_number(ch) || ('A' <= ch && ch <= 'F')
		|| ('a' <= ch && ch <= 'f');
}

static int
is_lowercase(char ch)
{
	return 'a' <= ch && ch <= 'z';
}

static void
skip_whitespace(void)
{
	char ch;
	while (is_whitespace(ch = fgetc(input))) {}
	ungetc(ch, input);
}

static token
skip_comment(void)
{
	token t;
	t.type = COMMENT;
	char ch;
	while ((ch=fgetc(input)) != '\n' && ch != EOF) {}
	if (ch==EOF)
		t.type = EOF_TOKEN;
	return t;
}

static token
read_pdf_version(void)
{
	int version = 0;
	char ch;

	if ((ch=fgetc(input)) != 'P')
		return skip_comment();
	if ((ch=fgetc(input)) != 'D')
		return skip_comment();
	if ((ch=fgetc(input)) != 'F')
		return skip_comment();
	if ((ch=fgetc(input)) != '-')
		return skip_comment();
	if ((ch=fgetc(input)) < '1' || ch > '2')
		return skip_comment();
	else
		version = 10*(ch-'0');
	if ((ch=fgetc(input)) != '.')
		return skip_comment();
	if ((ch=fgetc(input)) < '0' || ch > '7')
		return skip_comment();
	else
		version += ch-'0';
	
	token t = {PDF_VERSION_TOKEN, .val.intv = version};
	return t;
}

static token
read_pdf_eof(void)
{
	char ch;
	char buf[5] = "\%EOF";
	for (int i=0; i<4; i++) {
		if ((ch=fgetc(input)) != buf[i])
			return skip_comment();
	}
	token t;
	t.type = PDF_EOF_TOKEN;
	return t;
}


static token
read_comments(void)
{
	char ch = peek();
	fgetc(input);
	switch(ch = peek()) {
	case 'P':
		return read_pdf_version();
	case '%':
		return read_pdf_eof();
	default:
		return skip_comment();
	}
}

static char
read_octal(void)
{
	char first, second, third;
	first = fgetc(input); /* guaranteed octal */
	second = fgetc(input);
	if (!is_octal(second)) {
		ungetc(second, input);
		return first - '0';
	}
	third = fgetc(input);
	if (!is_octal(third)) {
		ungetc(third, input);
		return 8*(first-'0') + (second-'0');
	}
	return 64*(first-'0') + 8*(second-'0') + (third-'0');	
}

static token
read_string(void)
{
	token t;

	int paren_depth = 1;
	size_t index = 0;
	char ch = fgetc(input); /* guaranteed to be '(' */

	while (paren_depth > 0) {
		ch = fgetc(input);
		switch (ch) {
		case '\\':
			switch (ch=fgetc(input)) {
			case 'n':
				string_buffer[index] = '\n';
				break;
			case 'r':
				string_buffer[index] = '\r';
				break;
			case 't':
				string_buffer[index] = '\t';
				break;
			case 'b':
				string_buffer[index] = '\b';
				break;
			case 'f':
				string_buffer[index] = '\f';
				break;
			case '(':
				string_buffer[index] = '(';
				break;
			case ')':
				string_buffer[index] = ')';
				break;
			case '\\':
				string_buffer[index] = '\\';
				break;
			default:
				if (is_octal(ch)) {
					ungetc(ch, input);
					string_buffer[index] = read_octal();
				} else {
					err("Invalid escape sequence");
					t.type = LEX_ERROR_TOKEN;
					return t;
				}
			}
			break;
		
		case '(':
			paren_depth++;
			string_buffer[index] = '(';
			break;
		case ')':
			paren_depth--;
			string_buffer[index] = ')';
			break;
		case EOF:
			if (feof(input)) {
				t.type = LEX_ERROR_TOKEN;
				err("EOF reached in string");
				return t;
			}
			string_buffer[index] = ch;
			break;
		default:
			string_buffer[index] = ch;
		}

		index++;
	}
	string_buffer[index-1] = 0; /* eliminate closing parenthesis */

	t.type = STRING;
	t.val.str = str_from_buffer(string_buffer, index);
	t.length = index-1;
	return t;
}

static char
read_hex_char(char ch)
{
	/* assumption: is_hex(ch) */
	if (is_number(ch))
		return ch-'0';
	if (is_lowercase(ch))
		return 10+ch-'a';
	return 10+ch-'A';
}

static token
read_hex_string(void)
{
	token t;

	size_t index = 0, length = 0;
	char ch, to_add = 0;
	int is_first = 1;
	while ((ch=fgetc(input)) != '>') {
		if (is_whitespace(ch))
			continue;
		
		if (ch==EOF) {
			t.type = EOF_TOKEN;
			err("EOF reached in hexstring");
			return t;
		}

		if (!is_hex(ch)) {
			t.type = LEX_ERROR_TOKEN;
			err("Non-hexadecimal in hexstring");
			return t;
		}
		
		if (is_first) {
			to_add = 16*read_hex_char(ch);
			is_first = 0;
		}
		else {
			to_add += read_hex_char(ch);
			string_buffer[index++] = to_add;
			is_first = 1;
		}
	}

	if (!is_first) {
		string_buffer[index] = to_add;
		string_buffer[index+1] = 0;
		length = index+1;
	} else {
		string_buffer[index] = 0;
		length = index;
	}

	t.type = HEXSTRING;
	t.val.str = str_from_buffer(string_buffer, length);
	t.length = length;
	return t;
}

static token
read_name(void)
{
	token t;

	char aux1, aux2;
	char ch = fgetc(input); /* guaranteed '/' */
	size_t index = 0;
	ch = fgetc(input);
	int is_valid_char = is_regular(ch);
	while (is_valid_char) {
		
		if (ch=='#') {
			aux1 = fgetc(input);
			if (!is_hex(aux1)) {
			aux_error:
				t.type = LEX_ERROR_TOKEN;
				err("Invalid hex value in name");
				return t;
			}
			aux2 = fgetc(input);
			if (!is_hex(aux2))
				goto aux_error;
			
			regular_buffer[index++] =
				16*read_hex_char(aux1) + read_hex_char(aux2);
		}
		else {
			regular_buffer[index++] = ch;
		}


		ch = fgetc(input);
		is_valid_char = is_regular(ch);
	}
	ungetc(ch,input);
	regular_buffer[index] = 0;

	t.type = NAME;
	t.val.str = str_from_buffer(regular_buffer, index);
	return t;

}

static token
read_number(void)
{
	token t;
	long n = 0;
	double v = 0.0;

	size_t index = 0;
	int periodseen = 0;

	char ch = fgetc(input);
	int is_valid_char = 1; /* assumption for first character */
	if (ch=='.')
		periodseen = 1;
	
	while (is_valid_char) {
		regular_buffer[index++] = ch;

		ch = fgetc(input);
		is_valid_char = is_number(ch) || ch=='.';
		if (periodseen && ch=='.') {
			t.type = LEX_ERROR_TOKEN;
			err("Two periods in one number");
			return t;
		}
		if (ch=='.')
			periodseen = 1;
	}
	ungetc(ch, input);
	regular_buffer[index] = 0;

	if (periodseen) {
		v = strtod(regular_buffer, NULL);
		t.type = FLOAT;
		t.val.floatv = v;
		return t;
	}

	n = strtol(regular_buffer, NULL, 10);
	t.type = INTEGER;
	t.val.intv = n;
	return t;
}

static int
read_exactly(const char *str)
{
	char ch_to_read = (char) *(str++);
	while (ch_to_read != 0) {
		if (fgetc(input) != ch_to_read)
			return 0;
		ch_to_read = (char) *(str++);
	}
	return 1;
}

static token
read_keyword(void)
{
	token t;
	long pos = ftell(input);

	if (read_exactly("true")) {
		t.type = TRUE_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("false")) {
		t.type = FALSE_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("obj")) {
		t.type = OBJ_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("endobj")) {
		t.type = ENDOBJ_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("stream")) {
		t.type = STREAM_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("endstream")) {
		t.type = ENDSTREAM_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("xref")) {
		t.type = XREF_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("startxref")) {
		t.type = STARTXREF_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("trailer")) {
		t.type = TRAILER_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("R")) {
		t.type = R_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	if (read_exactly("null")) {
		t.type = NULL_KW;
		return t;
	}
	fseek(input, pos, SEEK_SET);

	t.type = LEX_ERROR_TOKEN;
	err("Unrecognized keyword");
	return t;
}


/**** parser ****/

typedef enum parse_res_type
{
	DIRECT_OBJ,
	INDIRECT_OBJ,
	XREF_TABLE,
	PDF_VERSION,
	PDF_EOF_REACHED,
	FILE_TRAILER,

	/* messages from a parser function to another */
	IO_ERROR_PARSER,
	LEX_ERROR_PARSER,
	PARSE_ERROR,
	EOF_REACHED,
} parse_res_type;

typedef struct parse_res {
	parse_res_type type;
	union {
		pag_object *obj;
		pag_ref	ref;
	} val;
	long pos;
} parse_res;


static parse_res *parse_integer_or_ref(void);
static parse_res *parse_float(void);
static parse_res *parse_bool(void);
static parse_res *parse_name(void);
static parse_res *parse_string(void);
static parse_res *parse_array(void);
static parse_res *parse_dict(void);
static void skip_comment_tokens(void);
static parse_res *result_direct_object(pag_object *obj);
static parse_res *result_parse_error(char *message);
static parse_res *result_lex_error(void);
static parse_res *result_io_error(void);

static parse_res *
parse_direct_object(void)
{
	skip_comment_tokens();

	parse_res *res = malloc(sizeof(parse_res));

	token t = peek_token();
	switch (t.type) {
	case INTEGER:
		return parse_integer_or_ref();
	case FLOAT:
		return parse_float();
	case TRUE_KW: case FALSE_KW:
		return parse_bool();
	case NAME:
		return parse_name();
	case STRING: case HEXSTRING:
		return parse_string();
	case LEFT_SQBRAC:
		return parse_array();
	case LTLT:
		return parse_dict();
	case GTGT:
		return result_parse_error("Closing '>>' with no matching '<<'");
	case RIGHT_SQBRAC:
		return result_parse_error("Closing ']' with no matching '['");
	case LEFT_CLBRAC:
	case RIGHT_CLBRAC:
		return result_parse_error(
			"Type 4 functions not yet supported");
	case NULL_KW:
		read_next();
		pag_object *obj = malloc(sizeof(pag_object));
		obj->type = PAG_NULL;
		return result_direct_object(obj);
	case OBJ_KW:
		return result_parse_error(
			"Expected direct object, got 'obj' keyword");
	case ENDOBJ_KW:
		return result_parse_error(
			"Expected direct object, got 'endobj' keyword");
	case STREAM_KW:
		return result_parse_error(
			"Expected direct object, got 'stream' keyword");
	case ENDSTREAM_KW:
		return result_parse_error(
			"Expected direct object, got 'endstream' keyword");
	case TRAILER_KW:
		return result_parse_error(
			"Expected direct object, got 'trailer' keyword");
	case XREF_KW:
		return result_parse_error(
			"Expected direct object, got 'xref' keyword");
	case STARTXREF_KW:
		return result_parse_error(
			"Expected direct object, got 'startxref' keyword");
	case R_KW:
		return result_parse_error(
			"Expected direct object, got 'R' keyword");
	case EOF_TOKEN:
		res->type = EOF_REACHED;
		return res;
	case PDF_EOF_TOKEN:
		res->type = PDF_EOF_REACHED;
		return res;
	case LEX_ERROR_TOKEN:
		return result_lex_error();
	case IO_ERROR_TOKEN:
		return result_io_error();
	default:
		return result_parse_error("This should be unreachable");
	}
}

static void
skip_comment_tokens(void)
{
	long pos = ftell(input);
	token t = read_next();
	while (t.type==COMMENT || t.type==PDF_VERSION_TOKEN
			|| t.type==PDF_EOF_TOKEN) {
		pos = ftell(input);
		t = read_next();
	}
	fseek(input, pos, SEEK_SET);
}

static parse_res *
result_direct_object(pag_object *obj)
{
	parse_res *res = malloc(sizeof(parse_res));
	res->type = DIRECT_OBJ;
	res->val.obj = obj;

	return res;
}

static parse_res *
result_parse_error(char *message)
{
	parse_res *res = malloc(sizeof(parse_res));
	res->type = PARSE_ERROR;
	err(message);
	return res;
}

static parse_res *
result_lex_error(void)
{
	parse_res *res = malloc(sizeof(parse_res));
	res->type = LEX_ERROR_PARSER;
	return res;
}

static parse_res *
result_io_error(void)
{
	parse_res *res = malloc(sizeof(parse_res));
	res->type = IO_ERROR_PARSER;
	return res;
}

static parse_res *
parse_integer_or_ref(void)
{
	token first = read_next(); /* guaranteed INTEGER */
	if (first.val.intv < 0) {
return_integer: ;
		pag_object *obj = pag_int2obj(pag_make_int(first.val.intv));
		return result_direct_object(obj);
	}
	
	long pos = ftell(input);
	token second = read_next();
	if (second.type != INTEGER || second.val.intv < 0) {
		fseek(input, pos, SEEK_SET);
		goto return_integer;
	}

	token third = read_next();
	if (third.type!=R_KW) {
		fseek(input, pos, SEEK_SET);
		goto return_integer;
	}

	pag_ref ref = pag_make_ref(first.val.intv, second.val.intv, NULL);
	return result_direct_object(pag_ref2obj(ref));
}

static parse_res *
parse_float(void)
{
	token t = read_next(); /* guaranteed FLOAT */
	pag_float f = pag_make_float(t.val.floatv);
	pag_object *obj = pag_float2obj(f);
	return result_direct_object(obj);
}

static parse_res *
parse_bool(void)
{
	token t = read_next();
	pag_bool b = pag_make_bool(t.type==TRUE_KW);
	pag_object *obj = pag_bool2obj(b);
	return result_direct_object(obj);
}

static parse_res *
parse_name(void)
{
	token t = read_next(); /* guaranteed NAME */
	pag_name name = pag_make_name(t.val.str);
	pag_object *obj = pag_name2obj(name);
	return result_direct_object(obj);
}

static parse_res *
parse_string(void)
{
	token t = read_next(); /* guaranteed STRING or HEXSTRING */
	pag_string str = pag_make_string(t.val.str, t.length);
	pag_object *obj = pag_string2obj(str);
	return result_direct_object(obj);
}

static parse_res *
parse_array(void)
{
	token t = read_next(); /* guaranteed LEFT_SQBRAC */
	pag_array *arr = pag_make_empty_array();
	while ((t=peek_token()).type != RIGHT_SQBRAC) {
		parse_res *res = parse_direct_object();
		if (res==NULL || res->type != DIRECT_OBJ)
			return res;
		arr = pag_array_append(arr, res->val.obj);
	}
	t = read_next(); /* guaranteed RIGHT_SQBRAC */

	return result_direct_object(pag_array2obj(arr));	
}

static parse_res *
parse_dict(void)
{
	token t = read_next(); /* guaranteed LTLT */
	pag_dict *dict = pag_make_empty_dict();
	while ((t=peek_token()).type != GTGT) {
		parse_res *res1 = parse_direct_object();
		if (res1==NULL || res1->type != DIRECT_OBJ)
			return res1;
		if (res1->val.obj->type != PAG_NAME)
			return result_parse_error(
				"Dictionary key must be name");

		t = peek_token();
		if (t.type==GTGT)
			return result_parse_error(
				"Premature end of dictionary");
		parse_res *res2 = parse_direct_object();
		if (res2==NULL || res2->type != DIRECT_OBJ)
			return result_parse_error(
				"Could not parse dictionary value");
		pag_dict_set(dict, res1->val.obj->val.name, res2->val.obj);
	}
	read_next(); /* GTGT */
	
	pag_object *obj = pag_dict2obj(dict);
	return result_direct_object(obj);
}

struct _stream_res {
	int err;
	union {
		char *str;
		parse_res_type errtype;
	} val;
};

static struct _stream_res
read_stream(size_t len)
{
	struct _stream_res res = {.err=0, .val.str=NULL};
	/* skip exactly one newline */
	char ch = fgetc(input);
	if (ch!='\r' && ch!='\n') {
	err_first_newline:
		err("Expected newline after 'stream' keyword");
		res.err = 1;
		res.val.errtype = PARSE_ERROR;
		return res;
	}
	if (ch=='\r') {
		ch = fgetc(input);
		if (ch!='\n')
			goto err_first_newline;
	}

	char *buf = calloc(len+1, 1); /* null-terminated for safety */

	for (size_t i=0; i<len; i++) {
		ch = fgetc(input);
		if (feof(input)) {
			err("Got EOF inside stream");
			res.err = 1;
			res.val.errtype = PARSE_ERROR;
			return res;
		}
		buf[i] = ch;
		/* TODO: verify EOD markers here */
	}

	token t = read_next();
	if (t.type != ENDSTREAM_KW) {
		//printf("%s\n", error_buffer);
		err("Expected 'endstream' keyword after stream");
		res.err = 1;
		res.val.errtype = PARSE_ERROR;
		return res;
	}

	res.val.str = buf;
	return res;
}

static parse_res *
parse_indirect_object(void)
{
	skip_comment_tokens();

	parse_res *res = malloc(sizeof(parse_res));
	res->type = INDIRECT_OBJ;

	token t1 = read_next();
	if (t1.type != INTEGER) {
	ind_obj_err:
		return result_parse_error("Expected indirect object");
	}
	token t2 = read_next();
	if (t2.type != INTEGER)
		goto ind_obj_err;

	token t3 = read_next();
	if (t3.type != OBJ_KW)
		goto ind_obj_err;
	
	parse_res *direct_res = parse_direct_object();
	if (direct_res->type != DIRECT_OBJ)
		return direct_res;
	
	token t4 = read_next();
	if (t4.type == STREAM_KW) {
		if (direct_res->val.obj->type != PAG_DICT)
			return result_parse_error("Stream with no dictionary");
		
		pag_object *lenobj = pag_dict_get(direct_res->val.obj->val.dict,
			 pag_make_name("Length"));
		if (lenobj==NULL)
			return result_parse_error(
				"Stream dictionary must contain /Length key");
		if (lenobj->type != PAG_INT || lenobj->val.intv.val < 0)
			return result_parse_error(
				"Stream length must be non-negative integer");
		
		int len = lenobj->val.intv.val;

		struct _stream_res stmres = read_stream((size_t)len);
		
		if (stmres.err) {
			res->type = stmres.val.errtype;
			return res;
		}

		pag_stream *stm = pag_make_stream(direct_res->val.obj->val.dict,
			stmres.val.str);
		
		token t5 = read_next();
		if (t5.type != ENDOBJ_KW)
			return result_parse_error("Expected 'endobj' keyword");
		
		pag_object *stmobj = pag_stream2obj(stm);
		pag_ref ref = pag_make_ref((unsigned)t1.val.intv,
			(unsigned)t2.val.intv, stmobj);
		res->val.ref = ref;

		return res;
	}
	if (t4.type != ENDOBJ_KW)
		return result_parse_error("Expected 'endobj' keyword");
	
	res->val.ref = pag_make_ref((unsigned int)t1.val.intv,
		(unsigned int)t2.val.intv, direct_res->val.obj);
	
	return res;
}

static long
get_xref_integer(token t)
{
	if (t.type != INTEGER) {
		err("Expected integer in xref");
		return -1;
	}
	if (t.val.intv < 0) {
		err("Expected positive integer in xref");
		return -1;
	}

	return (long)t.val.intv;	
}

static parse_res *
parse_xref_subsection(pag_xref_table *table)
{
	parse_res *res = malloc(sizeof(parse_res));
	res->type = PARSE_ERROR;

	long first = get_xref_integer(read_next());
	if (first < 0) return res;

	long len = get_xref_integer(read_next());
	if (len < 0) return res;

	if (first+len < (long)table->len)
		return result_parse_error(
			"Xref subsection does not fit in table");

	for (int i=first; i<first+len; i++) {
		long pos = get_xref_integer(read_next());
		if (pos<0) return res;
		long gen = get_xref_integer(read_next());
		if (gen<0) return res;

		int update = table->table[i].gen <= gen;

		if (update) {
			table->table[i].id = i;
			table->table[i].gen = gen;
			table->table[i].pos = pos;
		}

		skip_whitespace();
		char ch = fgetc(input);
		switch (ch) {
		case 'f':
			if (update)
				table->table[i].free = 1;
			break;
		case 'n':
			break;
		default:
			return result_parse_error(
				"Expected 'f' or 'n' in xref table");
		}
	}

	res->type = XREF_TABLE;
	return res;
}

static parse_res *
parse_xref(pag_xref_table *table)
{
	skip_comment_tokens();

	token t = read_next();
	if (t.type != XREF_KW)
		return result_parse_error("Expected 'xref' keyword");
	
	while (peek_token().type == INTEGER) {
		parse_res *res = parse_xref_subsection(table);
		if (res->type != XREF_TABLE)
			return res;
	}

	t = peek_token();
	if (t.type != TRAILER_KW)
		return result_parse_error("Expected 'trailer' keyword");
	
	parse_res *res = malloc(sizeof(parse_res));
	res->type = XREF_TABLE;

	return res;
}

static long
next_line_backwards(void)
{
	fseek(input, -1, SEEK_CUR);
	char ch = fgetc(input);
	while (ch != '\n') {
		fseek(input, -2, SEEK_CUR);
		ch = fgetc(input);
	}
	ungetc(ch, input);

	return ftell(input);
}

static long
find_trailer(void)
{
	fseek(input, -1, SEEK_END);

	token t;
	int trailerfound = 0;

	while (!trailerfound) {
		next_line_backwards();
		t = peek_token();
		trailerfound = (t.type==TRAILER_KW);
	}

	return ftell(input);
}

static parse_res *
parse_trailer(void)
{
	token t = read_next(); /* guaranteed TRAILER_KW */
	
	parse_res *res = parse_direct_object();

	if (res->type != DIRECT_OBJ || res->val.obj->type != PAG_DICT)
		return result_parse_error("Could not read trailer dictionary");
	res->type = FILE_TRAILER;

	t = read_next();
	if (t.type != STARTXREF_KW)
		return result_parse_error("Expected 'startxref' keyword");
	t = read_next();
	if (t.type != INTEGER || t.val.intv < 0)
		return result_parse_error(
			"Expected positive integer for startxref position");
	res->pos = t.val.intv;

	t = read_next();
	if (t.type != PDF_EOF_TOKEN)
		return result_parse_error("Expected '\%\%EOF' delimiter");
	
	return res;
}


/**** exported functions ****/

pag_object *
pag_parse(FILE *file)
{
	(void)file;
	return NULL; /* TODO */
}

pag_document *
pag_parse_file(FILE *file)
{
	pag_document *doc = malloc(sizeof(pag_document));
	doc->trailer_dicts = NULL;

	init_parser(file);

	skip_whitespace();
	token t = peek_token();
	if (t.type != PDF_VERSION_TOKEN) {
		err("Expected PDF version");
		return NULL;
	}
	doc->version = t.val.intv;
	doc->start_offset = ftell(input);

	find_trailer();
	parse_res *res = parse_trailer();


	if (res->type != FILE_TRAILER)
		return NULL;

	pag_dict *trailerdict = res->val.obj->val.dict;
	
	doc->trailer_dicts = pag_array_append(doc->trailer_dicts, res->val.obj);

	pag_object *obj;
	obj = pag_dict_get(trailerdict, pag_make_name("Size"));
	if (obj->type != PAG_INT || obj->val.intv.val < 1) {
		err("Expected integer >= 1 for /Size in file trailer");
		return NULL;
	}
	doc->len = obj->val.intv.val - 1;

	doc->table.len = doc->len+1;
	doc->table.table = calloc((size_t)doc->len+1, sizeof(pag_xref_entry));

	doc->objs = calloc((size_t)doc->len, sizeof(pag_object));

	long xrefpos = res->pos + doc->start_offset;

	fseek(input, xrefpos, SEEK_SET);
	res = parse_xref(&(doc->table));
	if (res->type != XREF_TABLE)
		return NULL;
	
	int all_trailers_read = 0;
	while (!all_trailers_read) {
		obj = pag_dict_get(trailerdict, pag_make_name("Prev"));
		if (obj == NULL) {
			all_trailers_read = 1;
			break;
		}
		if (obj->type != PAG_INT || obj->val.intv.val < 0) {
			err("Expected positive integer for /Prev");
			return NULL;
		}
		xrefpos = obj->val.intv.val + doc->start_offset;
		fseek(input, xrefpos, SEEK_SET);
		res = parse_xref(&(doc->table));
		if (res->type != XREF_TABLE)
			return NULL;
		res = parse_trailer();
		if(res->type != FILE_TRAILER)
			return NULL;
		trailerdict = res->val.obj->val.dict;
		pag_array_append(doc->trailer_dicts, res->val.obj);
	}

	for (int i=0; i<doc->len; i++) {
		long pos = doc->start_offset + doc->table.table[i+1].pos;
		fseek(input, pos, SEEK_SET);
		res = parse_indirect_object();
		if (res->type != INDIRECT_OBJ)
			return NULL;
		doc->objs[res->val.ref.id-1] = res->val.ref;
	}

	return doc;
}

char *
pag_parser_error(void)
{
	return error_buffer;
}

long
pag_parser_position(void)
{
	return error_pos;
}


/**** developement only ****/

void
print_token(token t)
{
	switch (t.type) {
	case INTEGER:
		printf("INTEGER %ld\n", t.val.intv);
		break;
	case FLOAT:
		printf("FLOAT %f\n", t.val.floatv);
		break;
	case STRING:
		printf("STRING (%s)\n", t.val.str);
		break;
	case HEXSTRING:
		printf("HEXSTRING <%s>\n", t.val.str);
		break;
	case LEFT_SQBRAC:
		printf("[\n"); break;
	case RIGHT_SQBRAC:
		printf("]\n"); break;
	case LEFT_CLBRAC:
		printf("{\n"); break;
	case RIGHT_CLBRAC:
		printf("}\n"); break;
	case PDF_EOF_TOKEN:
		printf("PDF_EOF_TOKEN\n"); break;
	case PDF_VERSION_TOKEN:
		printf("PDF_VERSION_TOKEN %ld\n", t.val.intv);
		break;
	case LTLT:
		printf("LTLT\n"); break;
	case GTGT:
		printf("GTGT\n"); break;
	case COMMENT:
		printf("COMMENT\n"); break;
	case NAME:
		printf("NAME /%s\n", t.val.str);
		break;
	case IO_ERROR_TOKEN:
		printf("IO_ERROR_TOKEN at %ld: %s\n", error_pos, error_buffer);
		break;
	case LEX_ERROR_TOKEN:
		printf("LEX_ERROR_TOKEN at %ld: %s\n", error_pos, error_buffer);
		break;
	case EOF_TOKEN:
		printf("EOF_TOKEN\n");
		break;
	case TRUE_KW:
		printf("TRUE\n"); break;
	case FALSE_KW:
		printf("FALSE\n"); break;
	case NULL_KW:
		printf("NULL_KW\n"); break;
	case OBJ_KW:
		printf("OBJ\n"); break;
	case ENDOBJ_KW:
		printf("ENDOBJ\n"); break;
	case STREAM_KW:
		printf("STREAM\n"); break;
	case ENDSTREAM_KW:
		printf("ENDSTREAM\n"); break;
	case XREF_KW:
		printf("XREF\n"); break;
	case STARTXREF_KW:
		printf("STARTXREF\n"); break;
	case TRAILER_KW:
		printf("TRAILER\n"); break;
	case R_KW:
		printf("R\n"); break;
	}
}

void
print_parse_res(parse_res *res)
{
	if (res==NULL) {
		printf("NULL\n");
		return;
	}
	switch (res->type) {
	case DIRECT_OBJ:
		printf("DIRECT_OBJ: ");
		pag_print_obj(res->val.obj);
		break;
	case INDIRECT_OBJ:
		printf("INDIRECT_OBJ: ");
		printf("%d %d R ", res->val.ref.id, res->val.ref.gen);
		pag_print_obj(res->val.ref.obj);
		break;
	case XREF_TABLE:
		printf("XREF_TABLE\n"); break;
	case PDF_VERSION:
		printf("PDF_VERSION\n"); break;
	case FILE_TRAILER:
		printf("FILE_TRAILER\n"); break;
	case IO_ERROR_PARSER:
		printf("IO_ERROR_PARSER at %ld: %s\n",
			error_pos, error_buffer);
		break;
	case LEX_ERROR_PARSER:
		printf("LEX_ERROR_PARSER at %ld: %s\n",
			error_pos, error_buffer);
		break;
	case PARSE_ERROR:
		printf("PARSE_ERROR at %ld: %s\n",
			error_pos, error_buffer);
		break;
	case EOF_REACHED: case PDF_EOF_REACHED:
		;

	}
}


/*
void
test_parser(FILE *file)
{
	init_parser(file);
	token t = read_next();
	int error_count = 0;
	do {
		print_token(t);
		if (t.type==IO_ERROR_TOKEN || t.type==LEX_ERROR_TOKEN ||
			t.type==EOF_TOKEN)
			error_count++;
		t = read_next();
	} while (error_count < 1);
}
*/

/*
void
test_parser(FILE *file)
{
	init_parser(file);
	parse_res *res = parse_direct_object();
	int error_count = 0;
	do {
		print_parse_res(res);
		if (res==NULL ||
		    res->type==IO_ERROR_PARSER ||
		    res->type==LEX_ERROR_PARSER ||
		    res->type==PARSE_ERROR ||
		    res->type==EOF_REACHED)
			error_count++;
		res = parse_direct_object();
	} while (error_count < 1);
}
*/

/*
void
test_parser(FILE *file)
{
	init_parser(file);
	parse_res *res = parse_indirect_object();
	int error_count = 0;
	do {
		print_parse_res(res);
		if (res==NULL ||
		    res->type==IO_ERROR_PARSER ||
		    res->type==LEX_ERROR_PARSER ||
		    res->type==PARSE_ERROR ||
		    res->type==EOF_REACHED)
			error_count++;
		res = parse_indirect_object();
	} while (error_count < 1);
} */