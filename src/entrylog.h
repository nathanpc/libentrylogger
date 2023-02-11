/**
 * entrylog.h
 * A library for parsing and easily working with Entrylog documents.
 *
 * @author Nathan Campos <nathan@innoveworkshop.com>
 */

#ifndef _ENTRYLOG_H
#define _ENTRYLOG_H

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#if !defined(__MSDOS__)
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#endif /* !__MSDOS__ */

#ifdef __MSDOS__
/* Integer types. */
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

/* Boolean. */
typedef uint8_t bool;
#define true 1
#define false 0
#endif /* __MSDOS__ */

#ifdef __cplusplus
extern "C" {
#endif

/* Utility macros. */
#define IF_EL_ERROR(err) if ((err) > EL_OK)

/* Sizes definitions. */
#define EL_FIELD_NAME_LEN 19

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
	char reserved;
	uint8_t type;
	uint16_t size_bytes;
	char name[EL_FIELD_NAME_LEN + 1];
} el_field_def_t;

/* Cell data abstraction. */
typedef struct {
	el_field_def_t *field;

	union {
		int32_t integer;
		float number;
		char *string;
	} value;
} el_cell_t;

/* Row data abstraction. */
typedef struct {
	uint32_t index;
	uint8_t cell_count;

	el_cell_t *cells;
} el_row_t;

/* EntryLogger document header. */
typedef struct {
	char magic[2];

	uint16_t header_len;
	uint16_t row_len;
	uint8_t field_desc_len;

	uint8_t field_desc_count;
	uint32_t row_count;

	char reserved[4];
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
el_err_t el_doc_read(eld_handle_t *doc, const char *fname);
el_err_t el_doc_save(eld_handle_t *doc, const char *fname);
el_err_t el_doc_field_add(eld_handle_t *doc, el_field_def_t field);
el_err_t el_doc_row_add(eld_handle_t *doc, el_row_t *row);
el_err_t el_doc_row_update(eld_handle_t *doc, const el_row_t *row);

/* Header operations. */
el_field_def_t el_field_def_new(el_type_t type, const char *name, uint16_t length);

/* Row operations. */
el_row_t *el_row_new(const eld_handle_t *doc);
el_row_t *el_row_get(eld_handle_t *doc, uint32_t index);
void el_row_free(el_row_t *row);

/* Utilities. */
uint16_t el_util_sizeof(el_type_t type);
bool el_util_file_exists(const char *fname);

/* Error handling. */
const char *el_error_msg(void);
void el_error_print(void);

#ifdef __cplusplus
}
#endif

#endif /* _ENTRYLOG_H */
