/**
 * libentrylogger Test Suite
 * A simple test application to ensure our libentrylogger library is working.
 *
 * @author Nathan Campos <nathan@innoveworkshop.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../src/entrylogger.h"

/* Private methods. */
void error_cleanup(eld_handle_t *doc);
el_err_t create_doc(eld_handle_t *doc, const char *fname);

int main(int argc, char **argv) {
	el_err_t err;
	eld_handle_t *doc;
	size_t i;

	/* Quick argument check. */
	if ((argc < 2) || (argc > 3)) {
		fprintf(stderr, "Usage: %s [-c] eldoc\n\n", argv[0]);
		fprintf(stderr, "    -c  Creates an example document.\n");
		return 1;
	}

	printf("libentrylogger Test Program\n\n");

	/* Initialize a new document handle object. */
	doc = el_doc_new();
	printf("New document handle object created.\n");

	/* Check if we need to create a new example document. */
	if (strcmp(argv[1], "-c") == 0) {
		err = create_doc(doc, argv[2]);
		IF_EL_ERROR(err) {
			error_cleanup(doc);
			return err;
		}

		goto quit;
	}

	/* Open an EntryLogger document. */
	err = el_doc_fopen(doc, argv[1], "rb");
	IF_EL_ERROR(err) {
		error_cleanup(doc);
		return err;
	}
	printf("EntryLogger document \"%s\" opened.\n", argv[1]);

	/* Parse the document. */
	err = el_doc_parse_header(doc);
	IF_EL_ERROR(err) {
		error_cleanup(doc);
		return err;
	}
	printf("Document successfully parsed.\n");

	/* Print the field definitions. */
	printf("Got %u field definitions!\n", doc->header.field_desc_count);
	for (i = 0; i < doc->header.field_desc_count; i++) {
		el_field_def_t field = doc->field_defs[i];
		printf("\t%u %s[%u]\n", field.type, field.name, field.size_bytes);
	}

quit:
	/* Close everything up. */
	err = el_doc_free(doc);
	IF_EL_ERROR(err) {
		el_error_print();
		return err;
	}
	printf("Document handle closed and free'd.\n");

	return 0;
}

/**
 * A simple helper function to clean things up after an error occurred and
 * inform the user.
 *
 * @param doc EntryLogger document object.
 */
void error_cleanup(eld_handle_t *doc) {
	el_error_print();
	el_doc_free(doc);
}

/**
 * Creates an example document to play around with.
 *
 * @param doc   EntryLogger document object.
 * @param fname Document file path.
 *
 * @return Error code returned by the called functions.
 */
el_err_t create_doc(eld_handle_t *doc, const char *fname) {
	el_err_t err;

	/* Add some sample fields. */
	printf("Adding sample fields...\n");
	err = el_doc_field_add(doc, el_field_def_new(EL_FIELD_INT, "Integer", 1));
	IF_EL_ERROR(err) {
		error_cleanup(doc);
		return err;
	}
	err = el_doc_field_add(doc, el_field_def_new(EL_FIELD_FLOAT, "Float", 1));
	IF_EL_ERROR(err) {
		error_cleanup(doc);
		return err;
	}
	err = el_doc_field_add(doc, el_field_def_new(EL_FIELD_STRING, "String 10", 10));
	IF_EL_ERROR(err) {
		error_cleanup(doc);
		return err;
	}
	printf("Finished adding sample fields.\n");

	/* Save the document. */
	err = el_doc_save(doc, fname);
	IF_EL_ERROR(err) {
		error_cleanup(doc);
		return err;
	}
	printf("EntryLogger document \"%s\" saved.\n", fname);

	return err;
}
