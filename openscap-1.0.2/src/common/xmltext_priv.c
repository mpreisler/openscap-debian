/*
 * Copyright 2013 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Authors:
 *	Šimon Lukašík
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xmltext_priv.h"
#include "_error.h"

int xmlTextReaderNextElement(xmlTextReaderPtr reader)
{
	__attribute__nonnull__(reader);

	int ret;
	do {
		ret = xmlTextReaderRead(reader);
		// if end of file
		if (ret < 1)
			break;
	} while (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT);

	if (ret == -1) {
		oscap_setxmlerr(xmlCtxtGetLastError(reader));
		/* TODO: Should we end here as fatal ? */
	}

	return ret;
}

int xmlTextReaderNextNode(xmlTextReaderPtr reader)
{
	__attribute__nonnull__(reader);

	int ret = xmlTextReaderRead(reader);
	if (ret == -1)
		oscap_setxmlerr(xmlGetLastError());

	return ret;
}

int xmlTextReaderNextElementWE(xmlTextReaderPtr reader, xmlChar* end_tag)
{
	__attribute__nonnull__(reader);

	int ret;
	do {
		ret = xmlTextReaderRead(reader);
		// if end of file
		if (ret < 1)
			break;

		if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) {
			if (!xmlStrcmp(xmlTextReaderConstLocalName(reader), end_tag)) {
				ret = 0;
				break;
			}
		}
	} while (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT);

	if (ret == -1) {
		oscap_setxmlerr(xmlCtxtGetLastError(reader));
		/* TODO: Should we end here as fatal ? */
	}

	return ret;
}
