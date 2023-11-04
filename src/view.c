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

void
pag_print_obj(pag_object *obj)
{
	switch (obj->type) {
	case PAG_STRING:
		printf("(");
		fwrite(obj->val.str.str, 1, obj->val.str.len, stdout);
		printf(")\n");
		break;
	case PAG_BOOL:
		printf("%d\n", obj->val.boolv.val);
		break;
	case PAG_ARRAY:
		printf("[\n");
		pag_array *arr = obj->val.array;
		while (arr != NULL) {
			printf("\t");
			pag_print_obj(arr->val);
			arr = arr->next;
		}
		printf("]\n");
		break;
	case PAG_DICT:
		printf("<<\n");
		pag_dict *dict = obj->val.dict;
		for (int i=0; i < (1<<dict->exp); i++) {
			if (dict->ht[i].obj != NULL) {
				printf("\t/%s ", dict->ht[i].key);
				pag_print_obj(dict->ht[i].obj);
			}
		}
		printf(">>\n");
		break;
	case PAG_FLOAT:
		printf("%f\n", obj->val.floatv.val);
		break;
	case PAG_INT:
		printf("%d\n", obj->val.intv.val);
		break;
	case PAG_NAME:
		printf("/%s\n", obj->val.name.str);
		break;
	case PAG_REF:
		printf("%d %d R\n", obj->val.ref.id, obj->val.ref.gen);
		break;
	case PAG_NULL:
		printf("null\n"); break;
	case PAG_STREAM:
		printf("stream ");
		pag_print_obj(pag_dict2obj(obj->val.stream->dict));
		break;

	}
}

void
pag_repl(pag_document *doc, FILE *output)
{
	char *cmd = calloc(1, 1000);
	
	printf(">>> ");
	while (fgets(cmd, 1000, stdin) != NULL) {
		if (cmd[0]=='v')
			printf("pdf version %d\n", doc->version);
		else if (cmd[0]=='q')
			break;
		else if (cmd[0]=='l') {
			printf("len = %d\n", doc->len);
		}
		else if (cmd[0]=='w') {
			pag_ref *ref = pag_get_info(doc);
			ref->obj = pag_make_info_dict();
			pag_set_object(doc, *ref);
			pag_write_document(doc, output);	
		}
		else if (cmd[0]=='r') {
			pag_ref *ref = pag_get_root(doc);
			pag_print_obj(pag_get_indirect_obj(doc, *ref));
		}
		else if (cmd[0]=='x') {
			int ref;
			if (sscanf(cmd, "x%d\n", &ref) == 0
			    || ref < 1 || ref > doc->len) {
				printf("Invalid object id\n>>> ");
				continue;
			}
			printf("offset = %ld, gen = %d\n",
				doc->table.table[ref].pos,
				doc->table.table[ref].gen);
		} else if (cmd[0]=='t') {
			pag_array *arr = doc->trailer_dicts;
			do {
				pag_print_obj(arr->val);
				arr = arr->next;
			} while (arr != NULL);
		} else {
			int ref;
			if (sscanf(cmd, "%d\n", &ref) == 0
			    || ref < 1 || ref > doc->len) {
				printf("Invalid object id\n");
				printf("\n>>> ");
				continue;
			}
			pag_print_obj(doc->objs[ref-1].obj);
		}
		printf("\n>>> ");
	}

	free(cmd);
}