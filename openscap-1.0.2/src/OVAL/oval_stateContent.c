/**
 * @file oval_stateContent.c
 * \brief Open Vulnerability and Assessment Language
 *
 * See more details at http://oval.mitre.org/
 */

/*
 * Copyright 2009--2013 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "David Niemoller" <David.Niemoller@g2-inc.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "oval_definitions_impl.h"
#include "adt/oval_collection_impl.h"
#include "oval_agent_api_impl.h"
#include "common/util.h"
#include "common/debug_priv.h"

typedef struct oval_state_content {
	struct oval_definition_model *model;
	struct oval_entity *entity;
	struct oval_collection *record_fields;
	oval_check_t ent_check;
	oval_check_t var_check;
} oval_state_content_t;

bool oval_state_content_iterator_has_more(struct oval_state_content_iterator
					  *oc_state_content)
{
	return oval_collection_iterator_has_more((struct oval_iterator *)
						 oc_state_content);
}

struct oval_state_content *oval_state_content_iterator_next(struct
							    oval_state_content_iterator
							    *oc_state_content)
{
	return (struct oval_state_content *)
	    oval_collection_iterator_next((struct oval_iterator *)
					  oc_state_content);
}

void oval_state_content_iterator_free(struct
				      oval_state_content_iterator
				      *oc_state_content)
{
	oval_collection_iterator_free((struct oval_iterator *)
				      oc_state_content);
}

struct oval_entity *oval_state_content_get_entity(struct oval_state_content
						  *content)
{
	__attribute__nonnull__(content);

	return content->entity;
}

struct oval_record_field_iterator *oval_state_content_get_record_fields(struct oval_state_content *content)
{
	if (!content->record_fields)
		return (struct oval_record_field_iterator *)
			oval_collection_iterator_new();
	return (struct oval_record_field_iterator *)
		oval_collection_iterator(content->record_fields);
}

oval_check_t oval_state_content_get_var_check(struct oval_state_content * content)
{
	__attribute__nonnull__(content);

	return content->var_check;
}

oval_check_t oval_state_content_get_ent_check(struct oval_state_content * content)
{
	__attribute__nonnull__(content);

	return content->ent_check;
}

struct oval_state_content *oval_state_content_new(struct oval_definition_model *model)
{
	oval_state_content_t *content = (oval_state_content_t *)
	    oscap_alloc(sizeof(oval_state_content_t));
	if (content == NULL)
		return NULL;

	content->entity = NULL;
	content->record_fields = oval_collection_new();
	content->ent_check = OVAL_CHECK_UNKNOWN;
	content->var_check = OVAL_CHECK_UNKNOWN;
	content->model = model;
	return content;
}

struct oval_state_content *oval_state_content_clone
    (struct oval_definition_model *new_model, struct oval_state_content *old_content) {
	struct oval_state_content *new_content = oval_state_content_new(new_model);
	oval_check_t echeck = oval_state_content_get_ent_check(old_content);
	oval_state_content_set_entcheck(new_content, echeck);
	struct oval_entity *entity = oval_state_content_get_entity(old_content);
	oval_state_content_set_entity(new_content, oval_entity_clone(new_model, entity));
	oval_check_t vcheck = oval_state_content_get_var_check(old_content);
	oval_state_content_set_varcheck(new_content, vcheck);
	return new_content;
}

void oval_state_content_free(struct oval_state_content *content)
{
	__attribute__nonnull__(content);

	if (content->entity)
		oval_entity_free(content->entity);
	if (content->record_fields)
		oval_collection_free_items(content->record_fields, (oscap_destruct_func) oval_record_field_free);
	oscap_free(content);
}

void oval_state_content_set_entity(struct oval_state_content *content, struct oval_entity *entity)
{
	__attribute__nonnull__(content);
	if (content->entity)
		oval_entity_free(content->entity);
	content->entity = entity;
}

void oval_state_content_add_record_field(struct oval_state_content *content, struct oval_record_field *rf)
{
	oval_collection_add(content->record_fields, rf);
}

void oval_state_content_set_varcheck(struct oval_state_content *content, oval_check_t check)
{
	__attribute__nonnull__(content);
	content->var_check = check;
}

void oval_state_content_set_entcheck(struct oval_state_content *content, oval_check_t check)
{
	__attribute__nonnull__(content);
	content->ent_check = check;
}

static void _oval_state_content_entity_consumer(struct oval_entity *entity, struct oval_state_content *content) {
	oval_state_content_set_entity(content, entity);
}

static void _oval_state_content_record_field_consumer(struct oval_record_field *rf, struct oval_state_content *content)
{
	oval_state_content_add_record_field(content, rf);
}

static int _oval_state_content_parse_record_field(xmlTextReaderPtr reader, struct oval_parser_context *context, void *user)
{
	return oval_record_field_parse_tag(reader, context, (oscap_consumer_func)
					   &_oval_state_content_record_field_consumer,
					   user, OVAL_RECORD_FIELD_STATE);
}

int oval_state_content_parse_tag(xmlTextReaderPtr reader,
				 struct oval_parser_context *context, oscap_consumer_func consumer, void *user)
{
	__attribute__nonnull__(context);

	oval_datatype_t ent_datatype;
	struct oval_state_content *content = oval_state_content_new(context->definition_model);
	int retcode = oval_entity_parse_tag
	    (reader, context, (oscap_consumer_func) _oval_state_content_entity_consumer, content);

	oval_check_t var_check = oval_check_parse(reader, "var_check", OVAL_CHECK_ALL);
	oval_check_t ent_check = oval_check_parse(reader, "entity_check", OVAL_CHECK_ALL);

	ent_datatype = oval_entity_get_datatype(content->entity);
	if (ent_datatype == OVAL_DATATYPE_RECORD) {
		retcode = oval_parser_parse_tag(reader, context,
			&_oval_state_content_parse_record_field, content);
	}

	oval_state_content_set_varcheck(content, var_check);
	oval_state_content_set_entcheck(content, ent_check);

	(*consumer) (content, user);
	return retcode;
}

xmlNode *oval_state_content_to_dom(struct oval_state_content * content, xmlDoc * doc, xmlNode * parent) {
	__attribute__nonnull__(content);

	struct oval_record_field_iterator *rf_itr;
	bool parent_mask;
	xmlNode *content_node = oval_entity_to_dom(content->entity, doc, parent);

	parent_mask = oval_entity_get_mask(content->entity);

	rf_itr = oval_state_content_get_record_fields(content);
	if (oval_record_field_iterator_has_more(rf_itr)) {
		xmlNsPtr field_ns = NULL;
		field_ns = xmlSearchNsByHref(doc, xmlDocGetRootElement(doc), OVAL_DEFINITIONS_NAMESPACE);
		if (field_ns == NULL) {
			field_ns = xmlNewNs(xmlDocGetRootElement(doc), OVAL_DEFINITIONS_NAMESPACE, BAD_CAST "oval-def");
		}

		while (oval_record_field_iterator_has_more(rf_itr)) {
			struct oval_record_field *rf;

			rf = oval_record_field_iterator_next(rf_itr);
			oval_record_field_to_dom(rf, parent_mask, doc, content_node, field_ns);
		}
	}
	oval_record_field_iterator_free(rf_itr);

	oval_check_t var_check = oval_state_content_get_var_check(content);
	if (var_check != OVAL_CHECK_ALL || xmlHasProp(content_node, BAD_CAST "var_ref"))
		xmlNewProp(content_node, BAD_CAST "var_check", BAD_CAST oval_check_get_text(var_check));

	oval_check_t ent_check = oval_state_content_get_ent_check(content);
	if (ent_check != OVAL_CHECK_ALL)
		xmlNewProp(content_node, BAD_CAST "entity_check", BAD_CAST oval_check_get_text(ent_check));

	return content_node;
}
