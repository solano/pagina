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
	/* ignore generation, just look for id */
	return doc->objs[ref.id-1].obj; /*FIXME: not correct*/
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

pag_object *
pag_get_object(pag_document *doc, pag_ref ref)
{
	if (doc==NULL)
		return NULL;
	return doc->objs[ref.id-1].obj;
}

void
pag_set_object(pag_document *doc, pag_ref ref)
{
	if (doc==NULL || ref.obj==NULL)
		return;
	
	doc->objs[ref.id-1].obj = ref.obj;
}