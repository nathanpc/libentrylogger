/**
 * entrylogger.h
 * A library for parsing and easily working with EntryLogger documents.
 *
 * @author Nathan Campos <nathan@innoveworkshop.com>
 */

#ifndef _ENTRYLOGGER_H
#define _ENTRYLOGGER_H

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Utility macros. */
#define IF_EL_ERROR(err) if ((err) > EL_OK)

/* Sizes definitions. */
#define EL_FIELD_NAME_LEN 20

/* EntryLogger parser status codes. */
typedef enum {
	EL_OK = 0,
	EL_ERROR_FILE,
	EL_ERROR_UNKNOWN,
	EL_ERROR_NOT_IMPL
} el_err_t;

/* Field types. */
typedef enum {
	EL_FIELD_INT = 0,
	EL_FIELD_FLOAT,
	EL_FIELD_STRING
} el_type_t;

/* Field descriptor. */
typedef struct {
	uint8_t type;
	uint16_t size_bytes;
	char name[EL_FIELD_NAME_LEN + 1];
} el_field_def_t;

/* Cell data abstraction. */
typedef struct {
	el_field_def_t *field;

	union {
		long integer;
		float number;
		char *string;
		void *pointer;
	} value;
} el_cell_t;

/* EntryLogger document header. */
typedef struct {
	char magic[3];

	uint16_t header_len;
	uint16_t row_len;
	uint8_t field_desc_len;

	uint8_t field_desc_count;
	uint32_t row_count;

	char marker[2];
} eld_header_t;

/* EntryLogger document handle. */
typedef struct {
	char *fname;
	FILE *fh;
	char fmode[4];

	eld_header_t header;
	el_field_def_t *field_defs;
} eld_handle_t;

/* EntryLogger document operations. */
eld_handle_t *el_doc_new(void);
el_err_t el_doc_fopen(eld_handle_t *doc, const char *fname, const char *fmode);
el_err_t el_doc_fclose(eld_handle_t *doc);
el_err_t el_doc_free(eld_handle_t *doc);
el_err_t el_doc_save(eld_handle_t *doc, const char *fname);
el_err_t el_doc_parse_header(eld_handle_t *doc);
el_err_t el_doc_field_add(eld_handle_t *doc, el_field_def_t field);

/* Header operations. */
el_field_def_t el_field_def_new(el_type_t type, const char *name, uint16_t length);

/* Error handling. */
const char *el_error_msg(void);
void el_error_print(void);

#ifdef __cplusplus
}
#endif

#endif /* _ENTRYLOGGER_H */
