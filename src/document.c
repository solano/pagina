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

#include "pagina.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

pag_ref *
pag_get_root(pag_document *doc)
{
	pag_dict *tdict = doc->trailer_dicts->val->val.dict;

	pag_object *obj = pag_dict_get(tdict, pag_make_name("Root"));

	if (obj==NULL || obj->type != PAG_REF)
		return NULL;
	
	return &(obj->val.ref);
}

pag_ref *
pag_get_info(pag_document *doc)
{
	pag_dict *tdict = doc->trailer_dicts->val->val.dict;

	pag_object *obj = pag_dict_get(tdict, pag_make_name("Info"));
	if (obj==NULL || obj->type != PAG_REF)
		return NULL;
	
	return &(obj->val.ref);
}

pag_object *
pag_get_indirect_obj(pag_document *doc, pag_ref ref)
{
	if (doc==NULL)
		return NULL;
	/* ignore generation, just look for id */
	return doc->objs[ref.id-1].obj;
}

void
pag_set_object(pag_document *doc, pag_ref ref)
{
	if (doc==NULL || ref.obj==NULL)
		return;
	
	doc->objs[ref.id-1].obj = ref.obj;
}

pag_object *
pag_make_info_dict(void)
{
	pag_dict *dict = pag_make_empty_dict();
	pag_name name = pag_make_name("Creator");
	pag_string str = pag_make_string("pagina", 6);
	pag_dict_set(dict, name, pag_string2obj(str));

	return pag_dict2obj(dict);
}



/****** PAGE LABELS *******/

/* the specification /C1/_2r8_4D_6D/A-/2 should give the following list of
page labels (with the corresponding page indices on the left):
0 - C1
1 - viii
2 - ix
3 - 1
4 - 2
5 - A-2
6 - A-3
7 - A-4
...

In the following, I write a small parser for this format. */

static char *plspec;
static size_t curindex, plspeclen;

static void
init_pl_lexer(char *spec)
{
	plspec = spec;
	curindex = 0;
	plspeclen = strlen(plspec);
}

static char
read_ch(void)
{
	char ch = plspec[curindex];
	if (curindex <= plspeclen)
		curindex++;
	return ch;
}

static char
peek_ch(void)
{
	return plspec[curindex];
}

enum pltokentype {
	PL_PREFIX,
	PL_NUMBER,
	PL_NUMTYPE,
	PL_ERROR,
	PL_EOS,
	PL_UNDERSCORE,
};

typedef struct {
	enum pltokentype type;
	union {
		char *str;
		int intv;
		char numtype;
	} val;
} pltoken;

static void
skip_underscores(void)
{
	char ch;
	while ((ch = read_ch()) == '_') {}
	curindex--;
}

static pltoken read_prefix(void);
static pltoken read_number(void);

static pltoken
read_token(void)
{
	pltoken t;

	char ch;
	switch (ch=peek_ch()) {
	case 'A': case 'a': case 'D': case 'R': case 'r':
		read_ch();
		t.type = PL_NUMTYPE;
		t.val.numtype = ch;
		return t;
	case '/':
		return read_prefix();
	case '\0':
		t.type = PL_EOS;
		return t;
	case '_':
		t.type = PL_UNDERSCORE;
		return t;
	default:
		if (isdigit(ch))
			return read_number();
		t.type = PL_ERROR;
		return t;
	}
}

#define PLBUFSIZE 30

static pltoken
read_prefix(void)
{
	pltoken t;

	char *buf = calloc(PLBUFSIZE+1, 1);
	size_t i = 0;

	char ch = read_ch(); /* guaranteed '/' */
	while ((ch=read_ch()) != '/') {
		buf[i++] = ch;
		if (i >= PLBUFSIZE) {
			t.type = PL_ERROR;
			return t;
		}
		if (!isgraph(ch)) {
			t.type = PL_ERROR;
			return t;
		}
	}

	t.type = PL_PREFIX;
	t.val.str = buf;
	return t;
}

pltoken
read_number(void)
{
	int val = 0;
	char ch;
	while (isdigit(ch=read_ch())) {
		val *= 10;
		val += (ch-'0');
	}
	curindex--;

	pltoken t = {.type=PL_NUMBER, .val.intv = val};
	return t;
}

typedef struct {
	int error;
	int index;
	int start;
	int prefixonly;
	char *prefix;
	char numtype;
} plrange;

plrange
read_plrange(int first)
{
	pltoken t1, t2, t3, t4;
	plrange r = {0};
	r.numtype = 'D';

	skip_underscores();

	switch ((t1=read_token()).type) {
	case PL_PREFIX:
		r.prefixonly = 1;
		r.prefix = t1.val.str;
		goto end;
	case PL_NUMBER:
		if (first) {
			r.error = 1;
			return r;
		}
		r.index = t1.val.intv - 1;
		break;
	case PL_NUMTYPE:
		if (!first) {
			r.error = 1;
			return r;
		}
		r.numtype = t1.val.numtype;
		goto read_t3;
	case PL_UNDERSCORE:
		goto end;
	case PL_ERROR: case PL_EOS:
		r.error = 1;
		return r;
	}

	switch ((t2=read_token()).type) {
	case PL_NUMTYPE:
		r.numtype = t2.val.numtype;
		break;
	case PL_EOS: case PL_UNDERSCORE:
		goto end;
	case PL_ERROR: case PL_NUMBER: case PL_PREFIX:
		r.error = 1;
		return r;
	}

read_t3:
	switch ((t3=read_token()).type) {
	case PL_PREFIX:
		r.prefix = t3.val.str;
		break;
	case PL_NUMBER:
		r.start = t3.val.intv;
		goto end;
	case PL_EOS: case PL_UNDERSCORE:
		goto end;
	case PL_ERROR: case PL_NUMTYPE:
		r.error = 1;
		return r;
	}

	switch ((t4=read_token()).type) {
	case PL_NUMBER:
		r.start = t4.val.intv;
		break;
	case PL_EOS: case PL_UNDERSCORE:
		goto end;
	default:
		r.error = 1;
		return r;
	}


end:
	/* require either nothing next or underscore */
	char ch = peek_ch();
	if (!(ch=='_' || ch=='\0'))
		r.error = 1;
	
	return r;
}

/* return zero on success */
int
add_range_to_array(pag_array **pl, plrange r)
{
	*pl = pag_array_append(*pl, pag_int2obj(pag_make_int(r.index)));

	pag_dict *dict = pag_make_empty_dict();

	if (r.prefix!=NULL) {
		pag_dict_set(dict, pag_make_name("P"),
		   pag_string2obj(pag_make_string(r.prefix, strlen(r.prefix))));
		if (r.prefixonly)
			goto end;
	}

	if (r.start != 0)
		pag_dict_set(dict, pag_make_name("St"),
		   pag_int2obj(pag_make_int(r.start)));
	
	char *buf = calloc(2, 1);
	buf[0] = r.numtype;
	pag_dict_set(dict, pag_make_name("S"),
		pag_name2obj(pag_make_name(buf)));

end:
	pag_array_append(*pl, pag_dict2obj(dict));
	return 0;
}

pag_object *
pag_make_pagelabels(char *spec)
{
	pag_dict *pagelabels = pag_make_empty_dict();
	pag_array *pl = pag_make_empty_array();
	init_pl_lexer(spec);

	plrange r = read_plrange(1);
	if (r.error)
		return NULL;
	if (add_range_to_array(&pl, r) != 0)
		return NULL;

	while (peek_ch()!='\0') {
		r = read_plrange(0);
		if (r.error)
			return NULL;
		if (add_range_to_array(&pl, r) != 0)
			return NULL;
	}

	pag_dict_set(pagelabels, pag_make_name("Nums"), pag_array2obj(pl));
	return pag_dict2obj(pagelabels);
}