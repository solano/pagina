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
#include "pagina.h"

static FILE *output;

static void
init_writer(FILE* file)
{
	output = file;
}

static int
contains_special_ch(char *str, unsigned len) {
	for (size_t i=0; i<len; i++) {
		if (str[i] < 32 || str[i] > 126)
			return 1;
	}
	return 0;
}

static void
write_hex_string(char *str, unsigned len) {
	fprintf(output, "<");
	for (size_t i=0; i<len; i++) {
		fprintf(output, "%02x", (unsigned char)str[i]);
	}
	fprintf(output, ">");
}

static void
write_obj(pag_object *obj, int nl, unsigned tl)
{
	/* nl = end with newline */
	/* tl = tab level */
	switch (obj->type) {
	case PAG_STRING:
		if (!contains_special_ch(obj->val.str.str, obj->val.str.len)) {
			fprintf(output, "(");
			fwrite(obj->val.str.str, 1, obj->val.str.len, output);
			fprintf(output, ")");
		} else {
			write_hex_string(obj->val.str.str, obj->val.str.len);
		}
		fprintf(output, nl?"\n":" ");
		break;
	case PAG_BOOL:
		fprintf(output, obj->val.boolv.val?"true":"false");
		fprintf(output, nl ? "\n" : " ");
		break;
	case PAG_ARRAY:
		fprintf(output, "[");
		pag_array *arr = obj->val.array;
		while (arr != NULL) {
			fprintf(output, " ");
			write_obj(arr->val, 0, 0);
			arr = arr->next;
		}
		fprintf(output, nl?"]\n":"] ");
		break;
	case PAG_DICT:
		fprintf(output, nl?"<<\n":"<<");
		pag_dict *dict = obj->val.dict;
		for (int i=0; i < (1<<dict->exp); i++) {
			if (dict->ht[i].obj != NULL) {
				if (nl)
					for (unsigned j=0; j<tl; j++)
						fprintf(output, "\t");
				fprintf(output, nl?"/%s ":" /%s ",
						dict->ht[i].key);
				write_obj(dict->ht[i].obj, 1, tl+1);
			}
		}
		if (nl)
			for (unsigned j=0; j<tl-1; j++)
				fprintf(output, "\t");
		fprintf(output, nl?">>\n":">> ");
		break;
	case PAG_FLOAT:
		fprintf(output, nl?"%f\n":"%f ", obj->val.floatv.val);
		break;
	case PAG_INT:
		fprintf(output, nl?"%d\n":"%d ", obj->val.intv.val);
		break;
	case PAG_NAME:
		fprintf(output, nl?"/%s\n":"/%s ", obj->val.name.str);
		break;
	case PAG_REF:
		fprintf(output, "%d %d R", obj->val.ref.id, obj->val.ref.gen);
		fprintf(output, nl?"\n":" ");
		break;
	case PAG_NULL:
		fprintf(output, nl?"null\n":"null "); break;
	case PAG_STREAM:
		write_obj(pag_dict2obj(obj->val.stream->dict), 1, tl);
		fprintf(output, "stream\n");
		fwrite(obj->val.stream->stream, 1, obj->val.stream->len, output);
		//fprintf(output, "STREAM GOES HERE");
		fprintf(output, "\nendstream\n");
		break;

	}
}

void
write_indirect_obj(pag_ref ref) {
	fprintf(output, "%d %d obj\n", ref.id, ref.gen);
	write_obj(ref.obj, 1, 1);
	fprintf(output, "endobj\n");
}

void
write_pdf_version(pag_document *doc) {
	fprintf(output, "%%PDF-%d.%d\n",
		doc->version/10, doc->version%10);
}

void
write_xref(unsigned long *arr, int len) {
	fprintf(output, "xref\n");
	fprintf(output, "0 %d\n", len+1);
	fprintf(output, "0000000000 65535 f");
	for (int i=0; i<len; i++) {
		fprintf(output, "%010ld 00000 n\n", arr[i]);
	}
}

void
write_trailer(unsigned long startxref, pag_object *dict) {
	fprintf(output, "trailer\n");
	write_obj(dict, 1, 1);
	fprintf(output, "startxref\n%ld\n%%%%EOF", startxref);
}

int
pag_write_document(pag_document *doc, FILE *file)
{
	init_writer(file);

	write_pdf_version(doc);

	unsigned long *arr = calloc(doc->len, sizeof(unsigned long));
	for (int i=0; i < doc->len; i++) {
		arr[i] = ftell(file);
		write_indirect_obj(doc->objs[i]);
	}

	unsigned long startxref = ftell(file);
	write_xref(arr, doc->len);
	write_trailer(startxref, doc->trailer_dicts->val);

	free(arr);
	return 0;
}