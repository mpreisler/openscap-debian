/*
 * Copyright 2012--2013 Red Hat Inc., Durham, North Carolina.
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
 *      Martin Preisler <mpreisle@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "public/scap_ds.h"
#include "public/oscap_text.h"
#include "public/oscap.h"

#include "common/alloc.h"
#include "common/_error.h"
#include "common/util.h"

#include "ds_common.h"

#include <sys/stat.h>
#include <time.h>
#include <libgen.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

static const char* arf_ns_uri = "http://scap.nist.gov/schema/asset-reporting-format/1.1";
static const char* core_ns_uri = "http://scap.nist.gov/schema/reporting-core/1.1";
static const char* arfvocab_ns_uri = "http://scap.nist.gov/specifications/arf/vocabulary/relationships/1.0#";
static const char* ai_ns_uri = "http://scap.nist.gov/schema/asset-identification/1.1";

static xmlNodePtr _lookup_container_in_arf(xmlDocPtr doc, const char *container_name)
{
	xmlNodePtr root = xmlDocGetRootElement(doc);
	xmlNodePtr ret = NULL;
	xmlNodePtr candidate = root->children;

	for (; candidate != NULL; candidate = candidate->next)
	{
		if (candidate->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char*)(candidate->name), container_name) == 0) {
			ret = candidate;
			break;
		}
	}

	return ret;
}

static xmlNodePtr _lookup_report_in_arf(xmlDocPtr doc, const char *report_id)
{
	xmlNodePtr container = _lookup_container_in_arf(doc, "reports");
	xmlNodePtr component = NULL;
	xmlNodePtr candidate = container->children;

	for (; candidate != NULL; candidate = candidate->next)
	{
		if (candidate->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char*)(candidate->name), "report") != 0)
			continue;

		char* candidate_id = (char*)xmlGetProp(candidate, BAD_CAST "id");
		if (strcmp(candidate_id, report_id) == 0)
		{
			component = candidate;
			xmlFree(candidate_id);
			break;
		}
		xmlFree(candidate_id);
	}

	return component;
}

static xmlNodePtr _lookup_request_in_arf(xmlDocPtr doc, const char *request_id)
{
	xmlNodePtr container = _lookup_container_in_arf(doc, "report-requests");
	xmlNodePtr component = NULL;
	xmlNodePtr candidate = container->children;

	for (; candidate != NULL; candidate = candidate->next)
	{
		if (candidate->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char*)(candidate->name), "report-request") != 0)
			continue;

		char* candidate_id = (char*)xmlGetProp(candidate, BAD_CAST "id");
		if (request_id == NULL || strcmp(candidate_id, request_id) == 0)
		{
			component = candidate;
			xmlFree(candidate_id);
			break;
		}
		xmlFree(candidate_id);
	}

	return component;
}

static xmlNodePtr ds_rds_get_inner_content(xmlDocPtr doc, xmlNodePtr parent_node)
{
	xmlNodePtr candidate = parent_node->children;
	xmlNodePtr content_node = NULL;

	for (; candidate != NULL; candidate = candidate->next)
	{
		if (candidate->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char*)(candidate->name), "content") == 0) {
			content_node = candidate;
			break;
		}
	}

	if (!content_node) {
		oscap_seterr(OSCAP_EFAMILY_XML, "Given ARF node has no 'arf:content' node inside!");
		return NULL;
	}

	return content_node;
}

static int ds_rds_dump_arf_content(xmlDocPtr doc, xmlNodePtr parent_node, const char* target_file)
{
	xmlNodePtr content_node = ds_rds_get_inner_content(doc, parent_node);

	if (!content_node)
		return -1;

	xmlNodePtr candidate = content_node->children;
	xmlNodePtr inner_root = NULL;

	for (; candidate != NULL; candidate = candidate->next)
	{
		if (candidate->type != XML_ELEMENT_NODE)
			continue;

		if (inner_root) {
			oscap_seterr(OSCAP_EFAMILY_XML, "There are multiple nodes inside an 'arf:content' node. "
				"Only the last one will be used!");
		}

		inner_root = candidate;
	}

	// We assume that arf:content is XML. This is reasonable because both
	// reports and report requests are XML documents.

	xmlDOMWrapCtxtPtr wrap_ctxt = xmlDOMWrapNewCtxt();

	xmlDocPtr new_doc = xmlNewDoc(BAD_CAST "1.0");
	xmlNodePtr res_node = NULL;
	if (xmlDOMWrapCloneNode(wrap_ctxt, doc, inner_root, &res_node, new_doc, NULL, 1, 0) != 0)
	{
		oscap_seterr(OSCAP_EFAMILY_XML, "Error when cloning node while dumping arf content.");
		xmlFreeDoc(new_doc);
		xmlDOMWrapFreeCtxt(wrap_ctxt);
		return -1;
	}
	xmlDocSetRootElement(new_doc, res_node);
	if (xmlDOMWrapReconcileNamespaces(wrap_ctxt, res_node, 0) != 0)
	{
		oscap_seterr(OSCAP_EFAMILY_XML, "Internal libxml error when reconciling namespaces while dumping arf content.");
		xmlFreeDoc(new_doc);
		xmlDOMWrapFreeCtxt(wrap_ctxt);
		return -1;
	}
	if (xmlSaveFileEnc(target_file, new_doc, "utf-8") == -1)
	{
		oscap_seterr(OSCAP_EFAMILY_GLIBC, "Error when saving resulting DOM to file '%s' while dumping arf content.", target_file);
		xmlFreeDoc(new_doc);
		xmlDOMWrapFreeCtxt(wrap_ctxt);
		return -1;
	}
	xmlFreeDoc(new_doc);

	xmlDOMWrapFreeCtxt(wrap_ctxt);

	return 0;
}

int ds_rds_decompose(const char* input_file, const char* report_id, const char* request_id, const char* target_dir)
{
	xmlDocPtr doc = xmlReadFile(input_file, NULL, 0);

	if (!doc) {
		oscap_seterr(OSCAP_EFAMILY_XML, "Could not read/parse XML of given input file at path '%s'.", input_file);
		return -1;
	}

	xmlNodePtr report_node = _lookup_report_in_arf(doc, report_id);
	if (!report_node) {
		const char* error = report_id ?
			oscap_sprintf("Could not find any report of id '%s'", report_id) :
			oscap_sprintf("Could not find any report inside the file");

		oscap_seterr(OSCAP_EFAMILY_XML, error);
		oscap_free(error);
		xmlFreeDoc(doc);
		return -1;
	}

	// make absolutely sure that the target dir exists
	// NOTE: if target dir already exists, this function returns 0
	if (ds_common_mkdir_p(target_dir) != 0) {
		oscap_seterr(OSCAP_EFAMILY_GLIBC, "Can't decompose RDS '%s' to target directory '%s'. "
			"Failed to create given directory!", input_file, target_dir);
		xmlFreeDoc(doc);
		return -1;
	}

	char *target_report_file = oscap_sprintf("%s/%s", target_dir, "report.xml");
	ds_rds_dump_arf_content(doc, report_node, target_report_file);
	oscap_free(target_report_file);

	xmlNodePtr request_node = _lookup_request_in_arf(doc, request_id);
	if (!request_node) {
		const char* error = request_id ?
			oscap_sprintf("Could not find any request of id '%s'", request_id) :
			oscap_sprintf("Could not find any request inside the file");

		oscap_seterr(OSCAP_EFAMILY_XML, error);
		oscap_free(error);
		xmlFreeDoc(doc);
		return -1;
	}

	char *target_request_file = oscap_sprintf("%s/%s", target_dir, "report-request.xml");
	ds_rds_dump_arf_content(doc, request_node, target_request_file);
	oscap_free(target_request_file);

	xmlFreeDoc(doc);
	return 0;
}

static xmlNodePtr ds_rds_create_report(xmlDocPtr target_doc, xmlNodePtr reports_node, xmlDocPtr source_doc, const char* report_id)
{
	xmlNsPtr arf_ns = xmlSearchNsByHref(target_doc, xmlDocGetRootElement(target_doc), BAD_CAST arf_ns_uri);

	xmlNodePtr report = xmlNewNode(arf_ns, BAD_CAST "report");
	xmlSetProp(report, BAD_CAST "id", BAD_CAST report_id);

	xmlNodePtr report_content = xmlNewNode(arf_ns, BAD_CAST "content");
	xmlAddChild(report, report_content);

	xmlDOMWrapCtxtPtr wrap_ctxt = xmlDOMWrapNewCtxt();
	xmlNodePtr res_node = NULL;
	xmlDOMWrapCloneNode(wrap_ctxt, source_doc, xmlDocGetRootElement(source_doc),
			&res_node, target_doc, NULL, 1, 0);
	xmlAddChild(report_content, res_node);
	xmlDOMWrapReconcileNamespaces(wrap_ctxt, res_node, 0);
	xmlDOMWrapFreeCtxt(wrap_ctxt);

	xmlAddChild(reports_node, report);

	return report;
}

static void ds_rds_add_relationship(xmlDocPtr doc, xmlNodePtr relationships,
		const char* type, const char* subject, const char* ref)
{
	xmlNsPtr core_ns = xmlSearchNsByHref(doc, xmlDocGetRootElement(doc), BAD_CAST core_ns_uri);

	// create relationship between given request and the report
	xmlNodePtr relationship = xmlNewNode(core_ns, BAD_CAST "relationship");
	xmlSetProp(relationship, BAD_CAST "type", BAD_CAST type);
	xmlSetProp(relationship, BAD_CAST "subject", BAD_CAST subject);

	xmlNodePtr ref_node = xmlNewNode(core_ns, BAD_CAST "ref");
	xmlNodeSetContent(ref_node, BAD_CAST ref);
	xmlAddChild(relationship, ref_node);

	xmlAddChild(relationships, relationship);
}

static xmlNodePtr ds_rds_add_ai_from_xccdf_results(xmlDocPtr doc, xmlNodePtr assets,
		xmlDocPtr xccdf_result_doc)
{
	xmlNsPtr arf_ns = xmlSearchNsByHref(doc, xmlDocGetRootElement(doc), BAD_CAST arf_ns_uri);
	xmlNsPtr ai_ns = xmlSearchNsByHref(doc, xmlDocGetRootElement(doc), BAD_CAST ai_ns_uri);

	xmlNodePtr asset = xmlNewNode(arf_ns, BAD_CAST "asset");

	// Lets figure out a unique asset identification
	// The format is: "asset%i" where %i is a increasing integer suffix
	//
	// We use a very simple optimization, we know that assets will be "ordered"
	// by their @id because we are adding them there in that order.
	// Whenever we get a collision we can simply bump the suffix and continue,
	// no need to go back and check the previous assets.

	xmlNodePtr child_asset = assets->children;

	unsigned int suffix = 0;
	for (; child_asset != NULL; child_asset = child_asset->next)
	{
		if (child_asset->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char*)(child_asset->name), "asset") != 0)
			continue;

		char* id_candidate = oscap_sprintf("asset%i", suffix);
		xmlChar* id = xmlGetProp(child_asset, BAD_CAST "id");

		if (strcmp(id_candidate, (const char*)id) == 0)
		{
			suffix++;
		}

		oscap_free(id_candidate);
	}

	char* id = oscap_sprintf("asset%i", suffix);
	xmlSetProp(asset, BAD_CAST "id", BAD_CAST id);
	oscap_free(id);

	xmlAddChild(assets, asset);

	xmlNodePtr computing_device = xmlNewNode(ai_ns, BAD_CAST "computing-device");
	xmlAddChild(asset, computing_device);

	xmlNodePtr connections = xmlNewNode(ai_ns, BAD_CAST "connections");
	xmlAddChild(computing_device, connections);

	xmlNodePtr test_result = xmlDocGetRootElement(xccdf_result_doc);

	xmlNodePtr test_result_child = test_result->children;

	for (; test_result_child != NULL; test_result_child = test_result_child->next)
	{
		if (test_result_child->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char*)(test_result_child->name), "target") == 0)
		{
			xmlChar* content = xmlNodeGetContent(test_result_child);
			xmlNewTextChild(computing_device, ai_ns, BAD_CAST "fqdn", content);
			xmlFree(content);
		}
		else if (strcmp((const char*)(test_result_child->name), "target-address") == 0)
		{
			xmlNodePtr connection = xmlNewNode(ai_ns, BAD_CAST "connection");
			xmlAddChild(connections, connection);
			xmlNodePtr ip_address = xmlNewNode(ai_ns, BAD_CAST "ip-address");
			xmlAddChild(connection, ip_address);

			xmlChar* content = xmlNodeGetContent(test_result_child);

			// we need to figure out whether the address is IPv4 or IPv6
			if (strchr((char*)content, '.') != NULL) // IPv4 has to have 4 dots
			{
				xmlNewTextChild(ip_address, ai_ns, BAD_CAST "ip-v4", content);
			}
			else // IPv6 has semicolons instead of dots
			{
				// lets expand the IPv6 to conform to the AI XSD and specification
				char *expanded_ipv6 = oscap_expand_ipv6((const char*)content);
				xmlNewTextChild(ip_address, ai_ns, BAD_CAST "ip-v6", BAD_CAST expanded_ipv6);
				oscap_free(expanded_ipv6);
			}
			xmlFree(content);
		}
	}

	return asset;
}

static int ds_rds_report_inject_ai_target_id_ref(xmlDocPtr doc, xmlNodePtr report, const char *asset_id)
{
	xmlNodePtr content_node = ds_rds_get_inner_content(doc, report);

	if (!content_node)
		return -1;

	if (!content_node->children) {
		oscap_seterr(OSCAP_EFAMILY_XML, "Given report doesn't contain any data, "
			"can't inject AI asset target id ref");
		return -1;
	}

	xmlNodePtr test_result_node = NULL;
	xmlNodePtr test_result_candidate = content_node->children;
	xmlNodePtr inner_element_node = NULL;

	while (test_result_candidate) {
		if (test_result_candidate->type == XML_ELEMENT_NODE) {
			inner_element_node = test_result_candidate;

			if (strcmp((const char*)test_result_candidate->name, "TestResult") == 0) {
				test_result_node = test_result_candidate;
				break;
			}
		}

		test_result_candidate = test_result_candidate->next;
	}

	if (!inner_element_node) {
		oscap_seterr(OSCAP_EFAMILY_XML, "Given report doesn't contain any XML element! "
			"Can't inject AI asset target id ref");
		return -1;
	}

	if (!test_result_node) {
		// TestResult may not be the top level element in the report.
		// While that is very unusual it is legitimate, lets check child elements.

		// As a rule, we only inject target-id-ref to the last test result
		// (XML, top-down).

		if (strcmp((const char*)inner_element_node->name, "Benchmark")) {
			oscap_seterr(OSCAP_EFAMILY_XML, "Top level element of the report isn't TestResult "
				"or Benchmark, the report is likely invalid!");
			return -1;
		}

		if (!inner_element_node->children) {
			oscap_seterr(OSCAP_EFAMILY_XML, "Top level element of the report isn't TestResult "
				"and does not contain any children! No TestResult to inject to has been found.");
			return -1;
		}

		test_result_candidate = inner_element_node->children;
		while (test_result_candidate) {
			if (test_result_candidate->type == XML_ELEMENT_NODE) {
				if (strcmp((const char*)test_result_candidate->name, "TestResult") == 0) {
					test_result_node = test_result_candidate;
					// we intentionally do not break here, we are looking for the
					// last (top-down) TestResult in the report.
					//break;
				}
			}

			test_result_candidate = test_result_candidate->next;
		}
	}

	if (!test_result_node) {
		oscap_seterr(OSCAP_EFAMILY_XML, "TestResult node to inject to has not been found"
			"(checked root element and all children of it).");
		return -1;
	}

	// Now we need to find the right place to inject the target-id-ref element.
	// It has to come after target, target-address and target-facts elements.
	// However target-address and target-fact are both optional.

	xmlNodePtr prev_sibling = NULL;
	xmlNodePtr prev_sibling_candidate = test_result_node->children;

	while (prev_sibling_candidate) {
		if (prev_sibling_candidate->type == XML_ELEMENT_NODE) {
			if (strcmp((const char*)prev_sibling_candidate->name, "target") == 0 ||
				strcmp((const char*)prev_sibling_candidate->name, "target-address") == 0 ||
				strcmp((const char*)prev_sibling_candidate->name, "target-facts") == 0) {

				prev_sibling = prev_sibling_candidate;
			}
		}

		prev_sibling_candidate = prev_sibling_candidate->next;
	}

	if (!prev_sibling) {
		oscap_seterr(OSCAP_EFAMILY_XML, "No target element was found in TestResult. "
			"The most likely reason is that the content is not valid! "
			"(XCCDF spec states 'target' element as required)");
		return -1;
	}

	// We have to make sure we are not injecting a target-id-ref that is there
	// already. if there is any duplicate, it has to come right after prev_sibling.
	xmlNodePtr duplicate_candidate = prev_sibling->next;
	while (duplicate_candidate) {
		if (duplicate_candidate->type == XML_ELEMENT_NODE) {
			if (strcmp((const char*)duplicate_candidate->name, "target-id-ref") == 0) {
				xmlChar* system_attr = xmlGetProp(duplicate_candidate, BAD_CAST "system");
				xmlChar* name_attr = xmlGetProp(duplicate_candidate, BAD_CAST "name");

				if (strcmp((const char*)system_attr, ai_ns_uri) == 0 &&
					strcmp((const char*)name_attr, asset_id) == 0) {

					xmlFree(system_attr);
					xmlFree(name_attr);
					return 0;
				}

				xmlFree(system_attr);
				xmlFree(name_attr);
			}
			else {
				break;
			}
		}
		duplicate_candidate = duplicate_candidate->next;
	}

	xmlNodePtr target_id_ref = xmlNewNode(prev_sibling->ns, BAD_CAST "target-id-ref");
	xmlNewProp(target_id_ref, BAD_CAST "system", BAD_CAST ai_ns_uri);
	xmlNewProp(target_id_ref, BAD_CAST "name", BAD_CAST asset_id);
	// @href is a required attribute by the XSD! The spec advocates filling it
	// blank when it's not needed.
	xmlNewProp(target_id_ref, BAD_CAST "href", BAD_CAST "");

	xmlAddNextSibling(prev_sibling, target_id_ref);

	return 0;
}

static void ds_rds_add_xccdf_test_results(xmlDocPtr doc, xmlNodePtr reports,
		xmlDocPtr xccdf_result_file_doc, xmlNodePtr relationships, xmlNodePtr assets,
		const char* report_request_id)
{
	xmlNodePtr root_element = xmlDocGetRootElement(xccdf_result_file_doc);

	// There are 2 possible scenarios here:

	// 1) root element of given xccdf result file doc is a TestResult element
	// This is the easier scenario, we will just use ds_rds_create_report and
	// be done with it.
	if (strcmp((const char*)root_element->name, "TestResult") == 0)
	{
		xmlNodePtr report = ds_rds_create_report(doc, reports, xccdf_result_file_doc, "xccdf1");
		ds_rds_add_relationship(doc, relationships, "arfvocab:createdFor",
				"xccdf1", report_request_id);

		xmlNodePtr asset = ds_rds_add_ai_from_xccdf_results(doc, assets, xccdf_result_file_doc);
		char* asset_id = (char*)xmlGetProp(asset, BAD_CAST "id");
		ds_rds_add_relationship(doc, relationships, "arfvocab:isAbout",
				"xccdf1", asset_id);
		xmlFree(asset_id);

		// We deliberately don't act on errors in inject ai target-id-ref as
		// these aren't fatal errors.
		ds_rds_report_inject_ai_target_id_ref(doc, report, asset_id);
	}

	// 2) the root element is a Benchmark, TestResults are embedded within
	// We will have to walk through all elements, wrap each TestResult
	// in a xmlDoc and add them separately
	else if (strcmp((const char*)root_element->name, "Benchmark") == 0)
	{
		unsigned int report_suffix = 1;

		xmlNodePtr candidate_result = root_element->children;

		for (; candidate_result != NULL; candidate_result = candidate_result->next)
		{
			if (candidate_result->type != XML_ELEMENT_NODE)
				continue;

			if (strcmp((const char*)(candidate_result->name), "TestResult") != 0)
				continue;

			xmlDocPtr wrap_doc = xmlNewDoc(BAD_CAST "1.0");

			xmlDOMWrapCtxtPtr wrap_ctxt = xmlDOMWrapNewCtxt();
			xmlNodePtr res_node = NULL;
			xmlDOMWrapCloneNode(wrap_ctxt, xccdf_result_file_doc, candidate_result,
					&res_node, wrap_doc, NULL, 1, 0);
			xmlDocSetRootElement(wrap_doc, res_node);
			xmlDOMWrapReconcileNamespaces(wrap_ctxt, res_node, 0);
			xmlDOMWrapFreeCtxt(wrap_ctxt);

			char* report_id = oscap_sprintf("xccdf%i", report_suffix++);
			xmlNodePtr report = ds_rds_create_report(doc, reports, wrap_doc, report_id);
			ds_rds_add_relationship(doc, relationships, "arfvocab:createdFor",
					report_id, report_request_id);

			xmlNodePtr asset = ds_rds_add_ai_from_xccdf_results(doc, assets, wrap_doc);
			char* asset_id = (char*)xmlGetProp(asset, BAD_CAST "id");
			ds_rds_add_relationship(doc, relationships, "arfvocab:isAbout",
					report_id, asset_id);

			// We deliberately don't act on errors in inject ai target-id-ref as
			// these aren't fatal errors.
			ds_rds_report_inject_ai_target_id_ref(doc, report, asset_id);

			xmlFree(asset_id);

			oscap_free(report_id);

			xmlFreeDoc(wrap_doc);
		}
	}

	else
	{
		char* error = oscap_sprintf(
				"Unknown root element '%s' in given XCCDF result document, expected TestResult or Benchmark.",
				(const char*)root_element->name);

		oscap_seterr(OSCAP_EFAMILY_XML, 0, error);
		oscap_free(error);
	}
}

static int ds_rds_create_from_dom(xmlDocPtr* ret, xmlDocPtr sds_doc, xmlDocPtr xccdf_result_file_doc, xmlDocPtr* oval_result_docs)
{
	*ret = NULL;

	xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
	xmlNodePtr root = xmlNewNode(NULL, BAD_CAST "asset-report-collection");
	xmlDocSetRootElement(doc, root);

	xmlNsPtr arf_ns = xmlNewNs(root, BAD_CAST arf_ns_uri, BAD_CAST "arf");
	xmlSetNs(root, arf_ns);

	xmlNsPtr core_ns = xmlNewNs(root, BAD_CAST core_ns_uri, BAD_CAST "core");
	xmlNewNs(root, BAD_CAST ai_ns_uri, BAD_CAST "ai");

	xmlNodePtr relationships = xmlNewNode(core_ns, BAD_CAST "relationships");
	xmlNewNs(relationships, BAD_CAST arfvocab_ns_uri, BAD_CAST "arfvocab");
	xmlAddChild(root, relationships);

	xmlNodePtr report_requests = xmlNewNode(arf_ns, BAD_CAST "report-requests");
	xmlAddChild(root, report_requests);

	xmlNodePtr assets = xmlNewNode(arf_ns, BAD_CAST "assets");
	xmlAddChild(root, assets);

	xmlNodePtr report_request = xmlNewNode(arf_ns, BAD_CAST "report-request");
	xmlSetProp(report_request, BAD_CAST "id", BAD_CAST "collection1");

	xmlNodePtr arf_content = xmlNewNode(arf_ns, BAD_CAST "content");

	xmlDOMWrapCtxtPtr sds_wrap_ctxt = xmlDOMWrapNewCtxt();
	xmlNodePtr sds_res_node = NULL;
	xmlDOMWrapCloneNode(sds_wrap_ctxt, sds_doc, xmlDocGetRootElement(sds_doc),
			&sds_res_node, doc, NULL, 1, 0);
	xmlAddChild(arf_content, sds_res_node);
	xmlDOMWrapReconcileNamespaces(sds_wrap_ctxt, sds_res_node, 0);
	xmlDOMWrapFreeCtxt(sds_wrap_ctxt);

	xmlAddChild(report_request, arf_content);

	xmlAddChild(report_requests, report_request);

	xmlNodePtr reports = xmlNewNode(arf_ns, BAD_CAST "reports");

	ds_rds_add_xccdf_test_results(doc, reports, xccdf_result_file_doc,
			relationships, assets, "collection1");

	unsigned int oval_report_suffix = 2;
	while (*oval_result_docs != NULL)
	{
		xmlDocPtr oval_result_doc = *oval_result_docs;

		char* report_id = oscap_sprintf("oval%i", oval_report_suffix++);
		ds_rds_create_report(doc, reports, oval_result_doc, report_id);
		oscap_free(report_id);

		oval_result_docs++;
	}

	xmlAddChild(root, reports);

	*ret = doc;
	return 0;
}

int ds_rds_create(const char* sds_file, const char* xccdf_result_file, const char** oval_result_files, const char* target_file)
{
	xmlDocPtr sds_doc = xmlReadFile(sds_file, NULL, 0);
	if (!sds_doc)
	{
		oscap_seterr(OSCAP_EFAMILY_XML, "Failed to read source datastream from '%s'.", sds_file);
		return -1;
	}

	xmlDocPtr result_file_doc = xmlReadFile(xccdf_result_file, NULL, 0);
	if (!result_file_doc)
	{
		oscap_seterr(OSCAP_EFAMILY_XML, "Failed to read XCCDF result file document from '%s'.", xccdf_result_file);
		xmlFreeDoc(sds_doc);
		return -1;
	}

	xmlDocPtr* oval_result_docs = oscap_alloc(1 * sizeof(xmlDocPtr));
	size_t oval_result_docs_count = 0;
	oval_result_docs[0] = NULL;

	int result = 0;
	// this check is there to allow passing NULL instead of having to allocate
	// an empty array
	if (oval_result_files != NULL)
	{
		while (*oval_result_files != NULL)
		{
			oval_result_docs[oval_result_docs_count] = xmlReadFile(*oval_result_files, NULL, 0);
			if (!oval_result_docs[oval_result_docs_count])
			{
				oscap_seterr(OSCAP_EFAMILY_XML, "Failed to read OVAL result file document from '%s'.", *oval_result_files);
				result = -1;
				continue;
			}

			oval_result_docs = oscap_realloc(oval_result_docs, (++oval_result_docs_count + 1) * sizeof(xmlDocPtr));
			oval_result_docs[oval_result_docs_count] = 0;
			oval_result_files++;
		}
	}

	xmlDocPtr rds_doc = NULL;
	// if reading OVAL docs failed at any point we won't create the RDS DOM
	if (result == 0)
		result = ds_rds_create_from_dom(&rds_doc, sds_doc, result_file_doc, oval_result_docs);

	// we won't even try to save the file if error happened when creating the DOM
	if (result == 0 && xmlSaveFileEnc(target_file, rds_doc, "utf-8") == -1)
	{
		oscap_seterr(OSCAP_EFAMILY_XML, "Failed to save the result datastream to '%s'.", target_file);
		result = -1;
	}
	xmlFreeDoc(rds_doc);

	xmlDocPtr* oval_result_docs_ptr = oval_result_docs;
	while (*oval_result_docs_ptr != NULL)
	{
		xmlFreeDoc(*oval_result_docs_ptr);
		oval_result_docs_ptr++;
	}

	oscap_free(oval_result_docs);

	xmlFreeDoc(sds_doc);
	xmlFreeDoc(result_file_doc);

	return result;
}