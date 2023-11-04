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


/* object types */
typedef struct pag_string	pag_string;
typedef struct pag_name		pag_name;
typedef struct pag_int		pag_int;
typedef struct pag_float	pag_float;
typedef struct pag_bool		pag_bool;
typedef struct pag_null		pag_null;
typedef struct pag_array	pag_array;
typedef struct pag_dict		pag_dict;
typedef enum pag_stmtype	pag_stmtype;
typedef struct pag_stream	pag_stream;

/* reference system */
typedef enum pag_objtype	pag_objtype;
typedef struct pag_object	pag_object;
typedef struct pag_ref		pag_ref;

/* data structures */


/* document semantics and file structure  */
typedef struct pag_page		pag_page;
typedef struct pag_pagetree	pag_pagetree;
typedef enum pag_pdf_version	pag_pdf_version;
typedef struct pag_trailer	pag_trailer;
typedef struct pag_xref_entry	pag_xref_entry;
typedef struct pag_xref_table	pag_xref_table;
typedef struct pag_document	pag_document;


/**** object types: methods ****/
char*		pag_read_string(pag_string str);
char*		pag_read_name(pag_name name);
long		pag_read_int(pag_int val);
double		pag_read_float(pag_float val);
int		pag_read_bool(pag_bool val);
pag_string	pag_make_string(char* str, size_t len);
pag_name	pag_make_name(char* name);
pag_int		pag_make_int(long val);
pag_float	pag_make_float(double val);
pag_bool	pag_make_bool(int val);
pag_ref		pag_make_ref(unsigned int id,
			unsigned int gen, pag_object *obj);
unsigned int	pag_array_len(pag_array *arr);

/* Get object from array. */
pag_object	*pag_array_get(pag_array *arr, int index);

/* Make empty array. */
pag_array	*pag_make_empty_array(void);

/* Make singleton array. */
pag_array	*pag_make_array_single(pag_object *obj);

/* Append object to array. */
pag_array	*pag_array_append(pag_array *arr, pag_object *obj);

/* Make empty dictionary. */
pag_dict	*pag_make_empty_dict(void);

/* Get value from dictionary. */
pag_object	*pag_dict_get(pag_dict *dict, pag_name name);

/* Get array of dictionary keys. */
pag_array	*pag_dict_get_keys(pag_dict *dict);

/* Set value in dictionary. */
int		pag_dict_set(pag_dict *dict, pag_name name, pag_object *obj);

/* Make stream object. */
pag_stream	*pag_make_stream(pag_dict *dict, char *buf);

/* Get type of stream. */
pag_stmtype	pag_stream_get_type(pag_stream *stream);

/* Get stream dictionary. */
pag_dict	*pag_stream_get_dict(pag_stream *stream);

/* Read stream, applying necessary filters. */
char		*pag_read_stream(pag_stream *stream);

/* Number of objects in an object stream. */
int		pag_objstm_nbobjs(pag_stream *stream);

/* Get id of first object in an object stream. */
int		pag_objstm_get_first_id(pag_stream *stream);

/* Get number of objects in object stream. */
unsigned int	pag_objstm_get_nbobjs(pag_stream *stream);

/* Get object in object stream by id. */
pag_object	*pag_objstm_get_obj(pag_stream *stream, int id);

/* Reindex reference in object stream. */
int		pag_objstm_reindex(pag_stream *stream, int oldid, int newid);

/* Expand an object stream, putting all its objects outside. */
int		pag_expand_objstm(pag_document *doc, pag_ref reference);

/* Expand all object streams in a document. */
int		pag_expand_all_objstm(pag_document *doc);

/* Make an object stream out of an array of references. */
pag_stream	*pag_make_objstm(pag_array references);

/* Contract n objects into an object stream. */
int		pag_contract_objstm(pag_document *doc, pag_ref first, int n);




/**** reference system: methods ****/
/* Get object type. */
pag_objtype	pag_object_get_type(pag_object *obj);

/* Extract object to which a reference points. */
pag_object	*pag_deref(pag_ref ref);

/* Dereference if this object is a reference, otherwise return object. */
pag_object	*pag_cond_deref(pag_object *obj);

/* Relabel a reference. */
int		pag_relabel_ref(pag_ref ref, unsigned int id,
				unsigned int gen);

pag_string	*pag_obj2string(pag_object *obj);
pag_object	*pag_string2obj(pag_string str);
pag_name	*pag_obj2name(pag_object *obj);
pag_object	*pag_name2obj(pag_name name);
pag_int		*pag_obj2int(pag_object *obj);
pag_object	*pag_int2obj(pag_int val);
pag_float	*pag_obj2float(pag_object *obj);
pag_object	*pag_float2obj(pag_float val);
pag_bool	*pag_obj2bool(pag_object *obj);
pag_object	*pag_bool2obj(pag_bool val);
pag_array	*pag_obj2array(pag_object *obj);
pag_object	*pag_array2obj(pag_array *arr);
pag_dict	*pag_obj2dict(pag_object *obj);
pag_object	*pag_dict2obj(pag_dict *dict);
pag_stream	*pag_obj2stream(pag_object *obj);
pag_object	*pag_stream2obj(pag_stream *stream);
pag_ref		*pag_obj2ref(pag_object *obj);
pag_object	*pag_ref2obj(pag_ref ref);

/**** document semantics and file structure: methods ****/
int		pag_pagetree_nbkids(pag_pagetree *tree);
int		pag_pagetree_nbpages(pag_pagetree *tree);
pag_page	*pag_pagetree_get_kid(pag_pagetree *tree, int index);
pag_page	*pag_pagetree_get_page(pag_pagetree *tree, int index);
pag_pagetree	*pag_get_pagetree(pag_document *doc);
pag_pagetree	*pag_make_pagetree(pag_page *pages[]);
pag_ref		*pag_get_root(pag_document *doc);
pag_ref		*pag_get_info(pag_document *doc);
pag_object	*pag_make_info_dict(void);
pag_object	*pag_get_indirect_obj(pag_document *doc, pag_ref ref);
void		pag_set_object(pag_document *doc, pag_ref ref);
int		pag_insert_objects(pag_object *objs[], pag_document *doc);
pag_document	*pag_make_document(pag_pdf_version version, pag_object *objs[]);


/**** parsing routines ****/
/* parse one object and advance the file position indicator. */
pag_object	*pag_parse(FILE *input);

/* parse the contents of an object stream and return array of objects. */
pag_array	*pag_parse_objstm(pag_stream *stream);

/* parse whole file, return NULL if it is invalid */
pag_document	*pag_parse_file(FILE *input);

long		pag_parser_position(void);
char		*pag_parser_error(void);

/**** writing routines ****/
char		*pag_obj2cstring(pag_object *obj);
int		pag_write_document(pag_document *doc, FILE *output);

/**** document structure visualization routines ****/
char		*pag_view_document(pag_document *doc, unsigned int level);
char		*pag_view_object(pag_object *obj, unsigned int level);
void		pag_print_obj(pag_object *obj);
void		pag_repl(pag_document *doc, FILE *output);


/***************
 * WARNING
 * From this point on, all code comprises implementation details and is
 * subject to change.
*/

/* object types: implementation */
struct pag_string
{
	size_t len;
	char *str; /* not null terminated */
};

struct pag_name
{
	char *str; /* null-terminated */
};

struct pag_int
{
	int val;
};

struct pag_float
{
	double val;
};

struct pag_bool
{
	int val;
};

struct pag_null {};

struct pag_array
{
	pag_object *val;
	pag_array *next;
};

struct _ht_entry
{
	char *key;
	pag_object *obj;
};

struct pag_dict
{
	size_t len;
	int exp;
	struct _ht_entry *ht;
};

struct pag_stream
{
	pag_dict *dict;
	unsigned long len;
	char *stream;
};

/* reference system: implementation */
enum pag_objtype
{
	PAG_STRING,
	PAG_NAME,
	PAG_INT,
	PAG_FLOAT,
	PAG_BOOL,
	PAG_NULL,
	PAG_ARRAY,
	PAG_DICT,
	PAG_STREAM,
	PAG_REF,
};

struct pag_ref
{
	unsigned int id;
	unsigned int gen;
	pag_object *obj;
};

struct pag_object
{
	pag_objtype type;
	union {
		pag_string	str;
		pag_name	name;
		pag_int		intv;
		pag_float	floatv;
		pag_bool	boolv;
		pag_null	nullv;
		pag_array	*array;
		pag_dict	*dict;
		pag_stream	*stream;
		pag_ref		ref;
	} val;
};

struct pag_xref_entry {
	int id;
	int gen;
	long pos;
	int free;
};

struct pag_xref_table {
	size_t len;
	pag_xref_entry *table;
};

struct pag_document
{
	int start_offset;
	int len;
	int version;
	pag_ref *objs;
	pag_array *trailer_dicts;
	pag_xref_table table;
};