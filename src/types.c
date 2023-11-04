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

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "pagina.h"

#define newobj() (malloc(sizeof(pag_object)))
#define newarr() (malloc(sizeof(pag_array)))

/**** object types: methods ****/
char*
pag_read_string(pag_string str)
{
	return str.str;
}

char*
pag_read_name(pag_name name)
{
	return name.str;
}

long
pag_read_int(pag_int val)
{
	return (long) val.val;
}

double
pag_read_float(pag_float val)
{
	return val.val;
}

int
pag_read_bool(pag_bool val)
{
	return val.val;
}

pag_string
pag_make_string(char* str, size_t len)
{
	pag_string newstr = {len, malloc(sizeof(char) * len)};
	strncpy(newstr.str, str, len);
	return newstr;
}

pag_name
pag_make_name(char* name)
{
	size_t len = strlen(name);
	pag_name newname = {calloc(len+1, 1)};
	strncpy(newname.str, name, len);
	return newname;
}

pag_int
pag_make_int(long val)
{
	pag_int newint = {val};
	return newint;
}

pag_float
pag_make_float(double val)
{
	pag_float newfloat = {val};
	return newfloat;
}

pag_bool
pag_make_bool(int val)
{
	pag_bool newbool = {val};
	return newbool;
}

pag_ref
pag_make_ref(unsigned int id, unsigned int gen, pag_object *obj)
{
	pag_ref ref = {.id=id, .gen=gen, .obj=obj};
	return ref;
}

unsigned int
pag_array_len(pag_array *arr)
{
	if (arr == NULL)
		return 0;
	return 1 + pag_array_len(arr->next);
}

pag_object *
pag_array_get(pag_array *arr, int index)
{
	if (arr == NULL)
		return NULL;
	if (index <= 0)
		return arr->val;
	return pag_array_get(arr->next, index-1);
}

pag_array *
pag_make_empty_array(void)
{
	return NULL;
}

pag_array *
pag_make_array_single(pag_object *obj)
{
	pag_array *arr = newarr();
	arr->next = NULL;
	arr->val = obj;
	return arr;
}

pag_array *
pag_array_append(pag_array *arr, pag_object *obj)
{
	if (arr == NULL)
		return pag_make_array_single(obj);
	if (arr->next == NULL) {
		arr->next = pag_make_array_single(obj);
		return arr;
	}
	pag_array_append(arr->next, obj);
	return arr;
}

/* "mask-step-index" hash table */
/* FIXME: should probably use fixed-size types... */
static int
ht_lookup(unsigned long hash, int exp, int idx)
{
	unsigned mask = ((unsigned)1 << exp) - 1;
	unsigned step = (hash >> (64-exp)) | 1;
	return (idx + step) & mask;
}

/* FNV-1a */
static unsigned long
hash_key(char *key, int len)
{
	unsigned long h = 0x100;
	for (int i=0; i<len; i++) {
		h ^= key[i] & 255;
		h *= 1111111111111111111;
	}
	return h ^ h>>32;
}

static pag_dict *
ht_new(int exp)
{
	pag_dict *ht = malloc(sizeof(pag_dict));
	ht->len = 0;
	ht->exp = exp;
	assert(exp >= 0);
	assert(exp < 32);
	ht->ht = calloc((size_t)1<<exp, sizeof(ht->ht[0]));
	return ht;
}

static int ht_insert(pag_dict *ht, struct _ht_entry entry);

static void
ht_rehash(pag_dict *ht)
{
	ht->exp++;
	ht->len = 0;
	assert(ht->exp < 32);
	struct _ht_entry *old_arr = (ht->ht);
	ht->ht = calloc((size_t)1<<ht->exp, sizeof(ht->ht[0]));

	for (int i=0; i<(1<<(ht->exp-1)); i++) {
		if (!old_arr[i].key)
			continue;
		ht_insert(ht, old_arr[i]);
	}

	free(old_arr);
}

pag_dict *
pag_make_empty_dict(void)
{
	return ht_new(3); /* initialize all dicts with capacity 8 */
}

/* return 1 if inserted, 0 if error */
static int
ht_insert(pag_dict *ht, struct _ht_entry entry)
{
	/* prevent filling up */
	if ((float)(ht->len)/(1<<ht->exp) >= 0.75)
		ht_rehash(ht);

	char *key = entry.key;
	unsigned long h = hash_key(key, strlen(key)+1);
	for (int i=h;;) {
		i = ht_lookup(h, ht->exp, i);
		if (!ht->ht[i].key) {
			/* empty slot found */
			ht->ht[i].key = key;
			ht->ht[i].obj = entry.obj;
			ht->len++;
			return 1;
		}

		if (!strcmp(ht->ht[i].key, key)) {
			/* slot with the same key found */
			ht->ht[i].obj = entry.obj;
			return 1;
		}
	}

	return 0; /* should be unreachable */
}

pag_object *
pag_dict_get(pag_dict *dict, pag_name name)
{
	char *key = name.str;
	unsigned long h = hash_key(key, strlen(key)+1);
	for (int i=h;;) {
		i = ht_lookup(h, dict->exp, i);
		if (!dict->ht[i].key) {
			/* empty slot found, give up search */
			return NULL;
		}

		if (!strcmp(dict->ht[i].key, key)) {
			/* key found */
			return dict->ht[i].obj;
		}
	}

	return NULL; /* should be unreachable */
}

int
pag_dict_set(pag_dict *dict, pag_name name, pag_object *obj)
{
	struct _ht_entry entry = {.key=name.str, .obj=obj};
	return ht_insert(dict, entry);
}

pag_stream *
pag_make_stream(pag_dict *dict, char *buf)
{
	pag_stream *stm = malloc(sizeof(pag_stream));
	stm->dict = dict;
	pag_object *lenobj = pag_dict_get(dict, pag_make_name("Length"));
	if (lenobj->type != PAG_INT || lenobj->val.intv.val < 0)
		return NULL;
	stm->len = lenobj->val.intv.val;
	stm->stream = buf;

	return stm;
}

pag_stmtype	pag_stream_get_type(pag_stream *stream);
pag_dict	*pag_stream_get_dict(pag_stream *stream);
char		*pag_read_stream(pag_stream *stream);
int		pag_objstm_nbobjs(pag_stream *stream);
int		pag_objstm_get_first_id(pag_stream *stream);
unsigned int	pag_objstm_get_nbobjs(pag_stream *stream);
pag_object	*pag_objstm_get_obj(pag_stream *stream, int id);
int		pag_objstm_reindex(pag_stream *stream, int oldid, int newid);
int		pag_expand_objstm(pag_document *doc, pag_ref reference);
int		pag_expand_all_objstm(pag_document *doc);
pag_stream	*pag_make_objstm(pag_array references);
int		pag_contract_objstm(pag_document *doc, pag_ref first, int n);




/**** reference system: methods ****/

pag_objtype	pag_object_get_type(pag_object *obj);
pag_object	*pag_deref(pag_ref ref);
pag_object	*pag_cond_deref(pag_object *obj);
int		pag_relabel_ref(pag_ref ref, unsigned int id,
				unsigned int gen);

pag_string *
pag_obj2string(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_STRING)
		return &(obj->val.str);
	return NULL;
}

pag_object *
pag_string2obj(pag_string str)
{
	pag_object *obj = newobj();
	obj->type = PAG_STRING;
	obj->val.str = str;
	return obj;
}

pag_name *
pag_obj2name(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_NAME)
		return &(obj->val.name);
	return NULL;
}

pag_object *
pag_name2obj(pag_name name)
{
	pag_object *obj = newobj();
	obj->type = PAG_NAME;
	obj->val.name = name;
	return obj;
}

pag_int *
pag_obj2int(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_INT)
		return &(obj->val.intv);
	return NULL;
}

pag_object *
pag_int2obj(pag_int val)
{
	pag_object *obj = newobj();
	obj->type = PAG_INT;
	obj->val.intv = val;
	return obj;
}

pag_float *
pag_obj2float(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_FLOAT)
		return &(obj->val.floatv);
	return NULL;
}

pag_object *
pag_float2obj(pag_float val)
{
	pag_object *obj = newobj();
	obj->type = PAG_FLOAT;
	obj->val.floatv = val;
	return obj;
}

pag_bool *
pag_obj2bool(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_BOOL)
		return &(obj->val.boolv);
	return NULL;
}

pag_object *
pag_bool2obj(pag_bool val)
{
	pag_object *obj = newobj();
	obj->type = PAG_BOOL;
	obj->val.boolv = val;
	return obj;
}

pag_array *
pag_obj2array(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_ARRAY)
		return obj->val.array;
	return NULL;
}

pag_object *
pag_array2obj(pag_array *arr)
{
	pag_object *obj = newobj();
	obj->type = PAG_ARRAY;
	obj->val.array = arr;
	return obj;
}

pag_dict *
pag_obj2dict(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_DICT)
		return obj->val.dict;
	return NULL;
}

pag_object *
pag_dict2obj(pag_dict *dict)
{
	pag_object *obj = newobj();
	obj->type = PAG_DICT;
	obj->val.dict = dict;
	return obj;
}

pag_stream *
pag_obj2stream(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_STREAM)
		return obj->val.stream;
	return NULL;
}

pag_object *
pag_stream2obj(pag_stream *stream)
{
	pag_object *obj = newobj();
	obj->type = PAG_STREAM;
	obj->val.stream = stream;
	return obj;
}

pag_ref *
pag_obj2ref(pag_object *obj)
{
	if (obj != NULL && obj->type == PAG_REF)
		return &(obj->val.ref);
	return NULL;
}

pag_object *
pag_ref2obj(pag_ref ref)
{
	pag_object *obj = newobj();
	obj->type = PAG_REF;
	obj->val.ref = ref;
	return obj;
}