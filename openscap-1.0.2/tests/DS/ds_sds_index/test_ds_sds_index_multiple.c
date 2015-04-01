/*
 * Copyright 2012 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * Authors:
 *      Martin Preisler <mpreisle@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scap_ds.h"
#include "common/list.h"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Invalid arguments, usage: ./test_ds_sds_index FILE");
		return 2;
	}

	struct ds_sds_index* idx = ds_sds_index_import(argv[1]);
	struct ds_stream_index_iterator* streams = ds_sds_index_get_streams(idx);

	// number of streams in the collection
	int nr_streams = 0;
	while (ds_stream_index_iterator_has_more(streams))
	{
		struct ds_stream_index* stream = ds_stream_index_iterator_next(streams);
		nr_streams++;

		if (nr_streams == 1 &&
		    strcmp(ds_stream_index_get_id(stream), "scap_org.open-scap_datastream_from_xccdf_scap-fedora14-xccdf.xml") != 0)
		{
			printf("Failed to read datastream ID correctly. "
			       "Expected 'scap_org.open-scap_datastream_from_xccdf_scap-fedora14-xccdf.xml', "
			       "found '%s'.\n", ds_stream_index_get_id(stream));
			return 1;
		}
		if (nr_streams == 2 &&
		    strcmp(ds_stream_index_get_id(stream), "scap_org.open-scap_datastream_from_xccdf_scap-fedora14-xccdf.xml_2") != 0)
		{
			printf("Failed to read datastream ID correctly. "
			       "Expected 'scap_org.open-scap_datastream_from_xccdf_scap-fedora14-xccdf.xml_2', "
			       "found '%s'.\n", ds_stream_index_get_id(stream));
			return 1;
		}
	}
	ds_stream_index_iterator_free(streams);

	if (ds_sds_index_get_stream(idx, "scap_org.open-scap_datastream_from_xccdf_scap-fedora14-xccdf.xml") == NULL)
	{
		printf("Attempted to retrieve 'scap_org.open-scap_datastream_from_xccdf_scap-fedora14-xccdf.xml' "
		       "by ID but got NULL as a result!\n");
		return 1;
	}
	if (ds_sds_index_get_stream(idx, "scap_org.open-scap_datastream_from_xccdf_scap-fedora14-xccdf.xml_2") == NULL)
	{
		printf("Attempted to retrieve 'scap_org.open-scap_datastream_from_xccdf_scap-fedora14-xccdf.xml_2' "
		       "by ID but got NULL as a result!\n");
		return 1;
	}
	if (ds_sds_index_get_stream(idx, "nonexistant_rubbish") != NULL)
	{
		printf("Attempted to retrieve a nonexistant stream by ID but got a non-NULL result!\n");
		return 1;
	}

	ds_sds_index_free(idx);

	if (nr_streams != 2)
	{
		printf("Expected to read 2 data-streams from the source datastream collection, "
		       "instead read %i!\n", nr_streams);
		return 1;
	}

	return 0;
}
