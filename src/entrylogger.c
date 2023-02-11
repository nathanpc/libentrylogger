/**
 * entrylogger.c
 * A library for parsing and easily working with EntryLogger documents.
 *
 * @author Nathan Campos <nathan@innoveworkshop.com>
 */

#ifdef __MSDOS__
#include "bcshim.h"
#include "entrylog.h"
#else
#include "entrylogger.h"
#endif /* __MSDOS__ */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __MSDOS__
#include <io.h>
#else
#include <unistd.h>
#endif /* __MSDOS__ */

/* Ensure that we have F_OK defined. */
#ifndef F_OK
	#define F_OK 0
#endif /* F_OK */

/* Decorate the error message with more information. */
#ifdef DEBUG
	#define STRINGIZE(x) STRINGIZE_WRAPPER(x)
	#define STRINGIZE_WRAPPER(x) #x
	#define EMSG(msg) msg " [" __FILE__ ":" STRINGIZE(__LINE__) "]"
	#define DEBUG_LOG(msg) \
		printf("[DEBUG] \"%s\" [" __FILE__ ":" STRINGIZE(__LINE__) "]\n", (msg))
#else
	#define EMSG(msg) msg
	#define DEBUG_LOG(msg) (void)0
#endif /* DEBUG */

/* Work with compilers that don't provide an vsnprintf implementation. */
#if !defined(vsnprintf) && !defined(VSNPRINTF_MAX_LEN)
	#define VSNPRINTF_MAX_LEN 255 /* Horrible, I know... */
#endif /* vsnprintf */

/* Private variables. */
static char *el_error_msg_buf = NULL;

/* Private methods. */
el_err_t el_doc_header_read(eld_handle_t *doc);
bool el_row_seek(eld_handle_t *doc, uint32_t index);
el_err_t el_row_read(el_row_t *row, eld_handle_t *doc, uint32_t index);
el_err_t el_doc_row_write(eld_handle_t *doc, const el_row_t *row);
size_t el_util_strcpy(char **dest, const char *src);
size_t el_util_strstrcpy(char **dest, const char *start, const char *end);
void el_util_calc_header_len(eld_handle_t *doc);
void el_util_calc_row_len(eld_handle_t *doc);
void el_error_free(void);
void el_error_msg_set(const char *msg);
void el_error_msg_format(const char *format, ...);

/**
 * Allocates a brand new EntryLogger document handle object.
 * @warning This function allocates memory that you are responsible for freeing.
 *
 * @return A brand new allocated EntryLogger document handle object.
 *
 * @see el_doc_free
 */
eld_handle_t *el_doc_new(void) {
	eld_handle_t *doc;

	/* Allocate our object. */
	doc = (eld_handle_t *)malloc(sizeof(eld_handle_t));

	/* Reset basics. */
	doc->fname = NULL;
	doc->fh = NULL;
	memset(doc->fmode, '\0', 4);

	/* Reset header definition. */
	doc->header.magic[0] = 'E';
	doc->header.magic[1] = 'L';
	doc->header.magic[2] = 'D';
	doc->header.marker[0] = '-';
	doc->header.marker[1] = '-';
	doc->header.field_desc_len = sizeof(el_field_def_t);
	doc->header.field_desc_count = 0;
	doc->header.row_count = 0;
	doc->field_defs = NULL;

	/* Calculate lengths. */
	el_util_calc_header_len(doc);
	el_util_calc_row_len(doc);

	return doc;
}

/**
 * Opens an existing or brand new EntryLogger document file.
 *
 * @param doc   Pointer to a EntryLogger document handle object.
 * @param fname Document file path or NULL if we should use the one defined in
 *              the object.
 * @param fmode File opening mode string. (see fopen)
 *
 * @return EL_OK if the operation was successful.
 *         EL_ERROR_FILE if an error occurred while trying to open the file.
 */
el_err_t el_doc_fopen(eld_handle_t *doc, const char *fname, const char *fmode) {
	/* Check if a document is still opened. */
	if (doc->fh != NULL) {
		el_error_msg_set(EMSG("A document is already open. Close it before "
							  "opening another one."));
		return EL_ERROR_FILE;
	}

	/* Have we been provided a new file to open? */
	if (fname != NULL) {
		/* Allocate space for the filename and copy it over. */
		doc->fname = (char *)realloc(doc->fname,
									(strlen(fname) + 1) * sizeof(char));
		strcpy(doc->fname, fname);
	}

	/* Set the file opening mode. */
	strncpy(doc->fmode, fmode, 3);

	/* Finally open the file. */
	doc->fh = fopen(doc->fname, doc->fmode);
	if (doc->fh == NULL) {
		el_error_msg_format(EMSG("Couldn't open file \"%s\": %s."), doc->fname,
							strerror(errno));
		return EL_ERROR_FILE;
	}

	return EL_OK;
}

/**
 * Closes the file handle for a EntryLogger document.
 *
 * @param doc EntryLogger document object to have its file handle closed.
 *
 * @return EL_OK if the operation was successful.
 *         EL_ERROR_FILE if an error occurred while trying to close the file.
 *
 * @see el_doc_free
 */
el_err_t el_doc_fclose(eld_handle_t *doc) {
	/* Do we even have anything to do? */
	if (doc->fh == NULL)
		return EL_OK;

	/* Try to close the file handle. */
	if (fclose(doc->fh) != 0) {
		el_error_msg_format(EMSG("Couldn't close file \"%s\": %s."),
							doc->fname, strerror(errno));
		return EL_ERROR_FILE;
	}

	/* NULL out the file handle and return. */
	doc->fh = NULL;
	return EL_OK;
}

/**
 * Frees up everything in the document object and closes the file handle. This
 * is what you want to call for a proper clean up.
 *
 * @param doc Document object to be completely cleaned up.
 */
el_err_t el_doc_free(eld_handle_t *doc) {
	el_err_t err;

	/* Start by closing the file handle. */
	err = el_doc_fclose(doc);
	IF_EL_ERROR(err) {
		return err;
	}

	/* Free file name. */
	free(doc->fname);

	/* Free the field definitions. */
	free(doc->field_defs);
	doc->field_defs = NULL;
	doc->header.field_desc_count = 0;

	return EL_OK;
}

/**
 * Reads the header and field definitions from a document file.
 *
 * @param doc   Pointer to a EntryLogger document handle object.
 * @param fname Document file path.
 *
 * @return EL_OK if the operation was successful.
 *         EL_ERROR_FILE if an error occurred while trying to read the file.
 */
el_err_t el_doc_read(eld_handle_t *doc, const char *fname) {
	el_err_t err;

	/* Open the document. */
	err = el_doc_fopen(doc, fname, "rb");
	IF_EL_ERROR(err) {
		return err;
	}

	/* Read the header and definitions from the file. */
	err = el_doc_header_read(doc);
	IF_EL_ERROR(err) {
		return err;
	}

	/* Close the document and return. */
	err = el_doc_fclose(doc);
	return err;
}

/**
 * Saves changes to a document header to a file.
 *
 * @param doc   Pointer to a EntryLogger document handle object.
 * @param fname Document file path or NULL if we should re-use the stored one.
 *
 * @return EL_OK if the operation was successful.
 *         EL_ERROR_FILE if an error occurred while operating on the file.
 *
 * @see el_doc_free
 */
el_err_t el_doc_save(eld_handle_t *doc, const char *fname) {
	el_err_t err;

	/* Open the document. */
	err = el_doc_fopen(doc, fname, "r+b");
	IF_EL_ERROR(err) {
		/* Try to create a new file if that was the previous issue. */
		err = el_doc_fopen(doc, fname, "wb");
		IF_EL_ERROR(err) {
			return err;
		}
	}

	/* Write the header to the file. */
	fwrite(&(doc->header), sizeof(eld_header_t), 1, doc->fh);

	/* Write field definitions to the file. */
	fwrite(doc->field_defs, sizeof(el_field_def_t),
		   doc->header.field_desc_count, doc->fh);

	/* Close the document and return. */
	err = el_doc_fclose(doc);
	return err;
}

/**
 * Parses a document's header and field definitions.
 *
 * @param doc Opened document handle.
 *
 * @return EL_OK if the operation was successful.
 *         EL_ERROR_FILE if something happened while trying to read the file.
 */
el_err_t el_doc_header_read(eld_handle_t *doc) {
	/* Read the header from the file. */
	fread(&(doc->header), sizeof(eld_header_t), 1, doc->fh);

	/* Allocate space for our field definitions and read them from the file. */
	doc->field_defs = (el_field_def_t *)realloc(
		doc->field_defs, sizeof(el_field_def_t) * doc->header.field_desc_count);
	fread(doc->field_defs, sizeof(el_field_def_t),
		  doc->header.field_desc_count, doc->fh);

	return EL_OK;
}

/**
 * Append a field definition to the document header.
 *
 * @param doc   Document handle.
 * @param field Field definition to be appended to the document.
 *
 * @return EL_OK if the operation was successful.
 */
el_err_t el_doc_field_add(eld_handle_t *doc, el_field_def_t field) {
	/* Make room for our new field definition. */
	doc->header.field_desc_count++;
	doc->field_defs = (el_field_def_t *)realloc(
		doc->field_defs, sizeof(el_field_def_t) * doc->header.field_desc_count);

	/* Copy the field over. */
	doc->field_defs[doc->header.field_desc_count - 1] = field;

	/* Re-calculate lengths. */
	el_util_calc_header_len(doc);
	el_util_calc_row_len(doc);

	return EL_OK;
}

/**
 * Writes a row structure to the file at the current position.
 *
 * @param doc Document object.
 * @param row Row to be written to the file.
 *
 * @return EL_OK if everything went fine.
 *         EL_ERROR_FILE if an error occurred while operating on the file.
 */
el_err_t el_doc_row_write(eld_handle_t *doc, const el_row_t *row) {
	uint8_t i;

	/* Go through the cells writing them. */
	for (i = 0; i < row->cell_count; i++) {
		size_t len;
		el_cell_t cell = row->cells[i];

		/* Write the cell. */
		len = cell.field->size_bytes;
		switch ((el_type_t)cell.field->type) {
			case EL_FIELD_INT:
				fwrite(&(cell.value.integer), len, 1, doc->fh);
				break;
			case EL_FIELD_FLOAT:
				fwrite(&(cell.value.number), len, 1, doc->fh);
				break;
			case EL_FIELD_STRING:
				fwrite(cell.value.string, len, 1, doc->fh);
				break;
		}

		/* Check if the write operation was successful. */
		if (ferror(doc->fh)) {
			el_error_msg_format(
				EMSG("Error occurred while trying to write cell %u at last "
					 "row: %s."),
				i, strerror(errno));
			return EL_ERROR_FILE;
		}
	}

	return EL_OK;
}

/**
 * Appends a new row to the end of the file.
 *
 * @param doc Document object.
 * @param row Row to be appended to the file.
 *
 * @return EL_OK if everything went fine.
 *         EL_ERROR_FILE if an error occurred while operating on the file.
 */
el_err_t el_doc_row_add(eld_handle_t *doc, el_row_t *row) {
	el_err_t err;

	/* Update the new row index and the header row count. */
	row->index = doc->header.row_count;
	doc->header.row_count++;

	/* Save the header changes. */
	err = el_doc_save(doc, NULL);
	IF_EL_ERROR(err) {
		return err;
	}

	/* Open the document for appending. */
	err = el_doc_fopen(doc, NULL, "a+b");
	IF_EL_ERROR(err) {
		return err;
	}

	/* Write row to the file. */
	err = el_doc_row_write(doc, row);
	IF_EL_ERROR(err) {
		return err;
	}

	/* Close the document and return. */
	err = el_doc_fclose(doc);
	return err;
}

/**
 * Updates an existing row in the file.
 *
 * @param doc Document object.
 * @param row Row to be updated in the file.
 *
 * @return EL_OK if everything went fine.
 *         EL_ERROR_FILE if an error occurred while operating on the file.
 */
el_err_t el_doc_row_update(eld_handle_t *doc, const el_row_t *row) {
	el_err_t err;

	/* Open the document for updating. */
	err = el_doc_fopen(doc, NULL, "r+b");
	IF_EL_ERROR(err) {
		return err;
	}

	/* Seek to the right position. */
	if (!el_row_seek(doc, row->index)) {
		return EL_ERROR_FILE;
	}

	/* Write row to the file. */
	err = el_doc_row_write(doc, row);
	IF_EL_ERROR(err) {
		return err;
	}

	/* Close the document and return. */
	err = el_doc_fclose(doc);
	return err;
}

/**
 * Creates a brand new field definition.
 *
 * @param type   Type of the field data.
 * @param name   Name of the field.
 * @param length Length of the field. Set to 1 always, except for strings.
 *
 * @return A populated field definition structure.
 */
el_field_def_t el_field_def_new(el_type_t type, const char *name, uint16_t length) {
	el_field_def_t field;

	/* Set the basics. */
	field.type = (uint8_t)type;
	field.size_bytes = el_util_sizeof(type) * length;

	/* Make sure we allocate enough space for strings. */
	if (field.type == EL_FIELD_STRING)
		field.size_bytes += el_util_sizeof(type);

	/* Copy the name over. */
	memset(field.name, '\0', EL_FIELD_NAME_LEN + 1);
	strncpy(field.name, name, EL_FIELD_NAME_LEN);

	return field;
}

/**
 * Creates a brand new allocated row object.
 * @warning This function allocates memory that you are responsible for freeing.
 *
 * @param doc Document handle.
 *
 * @return Brand new allocated row object.
 *
 * @see el_row_get
 */
el_row_t *el_row_new(const eld_handle_t *doc) {
	el_row_t *row;
	uint8_t i;

	/* Allocate memory for our row structure and populate some of it. */
	row = (el_row_t *)malloc(sizeof(el_row_t));
	row->index = doc->header.row_count;
	row->cell_count = doc->header.field_desc_count;

	/* Allocate memory for our cells and prepare them to receive data. */
	row->cells = (el_cell_t *)malloc(sizeof(el_cell_t) * row->cell_count);
	for (i = 0; i < row->cell_count; i++) {
		el_cell_t *cell = &(row->cells[i]);

		/* Populate the field definition. */
		cell->field = &(doc->field_defs[i]);

		/* Allocate space for types that need it. */
		switch (cell->field->type) {
			case EL_FIELD_STRING:
				cell->value.string = (char *)malloc(cell->field->size_bytes);
				memset(cell->value.string, '\0', cell->field->size_bytes);
				break;
			default:
				break;
		}
	}

	return row;
}

/**
 * Seeks to the beginning of a row in the file given its index.
 *
 * @param doc   Opened document object.
 * @param index Index of the row to seek to.
 *
 * @return TRUE if the operation was successful.
 *         FALSE otherwise (check ferror for more information).
 */
bool el_row_seek(eld_handle_t *doc, uint32_t index) {
	/* Determine the offset that the row is located at. */
	long offset = doc->header.header_len + (doc->header.row_len * index);

	/* Try to seek to the row offset. */
	if (fseek(doc->fh, offset, SEEK_SET) != 0) {
		el_error_msg_format(
			EMSG("Couldn't seek in file \"%s\": %s."),
			doc->fname, strerror(errno));
		return false;
	}

	return true;
}

/**
 * Reads the contents of a row from the file into a row object.
 * @warning This function allocates memory that you are responsible for freeing.
 *
 * @param row   Prepared row object.
 * @param doc   Document object.
 * @param index Index of the row to be read.
 *
 * @return EL_OK if everything went fine.
 *         EL_ERROR_FILE if an error occurred while operating on the file.
 *
 * @see el_row_get
 */
el_err_t el_row_read(el_row_t *row, eld_handle_t *doc, uint32_t index) {
	el_err_t err;
	uint8_t i;

	/* Open the document. */
	err = el_doc_fopen(doc, NULL, "rb");
	IF_EL_ERROR(err) {
		return err;
	}

	/* Seek to the beginning of the rows section. */
	if (!el_row_seek(doc, index))
		return EL_ERROR_FILE;

	/* Populate the cells */
	for (i = 0; i < row->cell_count; i++) {
		size_t len;
		el_cell_t *cell = &(row->cells[i]);

		/* Read the cell into our structure. */
		len = cell->field->size_bytes;
		switch ((el_type_t)cell->field->type) {
			case EL_FIELD_INT:
				fread(&(cell->value.integer), len, 1, doc->fh);
				break;
			case EL_FIELD_FLOAT:
				fread(&(cell->value.number), len, 1, doc->fh);
				break;
			case EL_FIELD_STRING:
				fread(cell->value.string, len, 1, doc->fh);
				break;
		}

		/* Check if the read operation was successful. */
		if (feof(doc->fh)) {
			/* EOF reached. */
			el_error_msg_format(
				EMSG("End-of-file reached before we could finish reading "
						"cell %u at row %lu."), i, index);
			return EL_ERROR_FILE;
		} else if (ferror(doc->fh)) {
			/* Error reading. */
			el_error_msg_format(
				EMSG("Error occurred while trying to read cell %u at row "
						"%lu: %s."), i, index, strerror(errno));
			return EL_ERROR_FILE;
		}
	}

	/* Close the document and return. */
	err = el_doc_fclose(doc);
	return err;
}

/**
 * Gets a row from a document at a specified index.
 * @warning This function allocates memory that you are responsible for freeing.
 *
 * @param doc   Document handle.
 * @param index Index of the row.
 *
 * @return Requested row (allocated memory) or NULL if one wasn't found.
 *
 * @see el_row_new
 */
el_row_t *el_row_get(eld_handle_t *doc, uint32_t index) {
	el_err_t err;
	el_row_t *row;

	/* Check if the index is valid. */
	if (index >= doc->header.row_count) {
		el_error_msg_format(EMSG("Requested index %lu is greater than the "
								 "number of rows (%lu) in the document."),
							index, doc->header.row_count);
		return NULL;
	}

	/* Create a new row object and set its index. */
	row = el_row_new(doc);
	row->index = index;

	/* Populate the row object with cells. */
	err = el_row_read(row, doc, index);
	IF_EL_ERROR(err) {
		el_row_free(row);
		return NULL;
	}

	return row;
}

/**
 * Frees up any resources allocated by a row object.
 *
 * @param row Row object to be free'd.
 */
void el_row_free(el_row_t *row) {
	uint8_t i;

	/* Should we do anything? */
	if (row == NULL)
		return;

	/* Free any cell contents that were allocated. */
	for (i = 0; i < row->cell_count; i++) {
		switch (row->cells[i].field->type) {
			case EL_FIELD_STRING:
				/* Strings are allocated */
				free(row->cells[i].value.string);
				row->cells[i].value.string = NULL;

				break;
			default:
				break;
		}
	}

	/* Ensure that all of our counters are reset. */
	row->index = 0;
	row->cell_count = 0;

	/* Free the cells. */
	if (row->cells == NULL)
		return;
	free(row->cells);
	row->cells = NULL;

	/* Free ourselves. */
	free(row);
	row = NULL;
}

/**
 * Calculates the length of the file header based on the field descriptor length
 * and the number of fields defined.
 *
 * @param doc Document handle.
 */
void el_util_calc_header_len(eld_handle_t *doc) {
	doc->header.header_len =
		(sizeof(el_field_def_t) * doc->header.field_desc_count) +
		sizeof(eld_header_t);
}

/**
 * Calculates the fixed length of a single row in a document given its header
 * parameters.
 *
 * @param doc Document handle.
 */
void el_util_calc_row_len(eld_handle_t *doc) {
	uint8_t i;

	/* Check if we have the necessary data to perform calculations. */
	if (doc->field_defs == NULL) {
		doc->header.row_len = 0;
		doc->header.row_count = 0;

		return;
	}

	/* Go through field definitions calculating their individual lengths. */
	doc->header.row_len = 0;
	for (i = 0; i < doc->header.field_desc_count; i++) {
		doc->header.row_len += doc->field_defs[i].size_bytes;
	}
}

/**
 * Gets the size of a single instance of a type of variable in bytes.
 *
 * @param type Type of variable.
 *
 * @return Length in bytes that a single instance of this type occupies.
 */
uint16_t el_util_sizeof(el_type_t type) {
	switch (type) {
		case EL_FIELD_INT:
			return sizeof(long);
		case EL_FIELD_FLOAT:
			return sizeof(float);
		case EL_FIELD_STRING:
			return sizeof(char);
	}

	return 0;
}

/**
 * Sets the error message that the user can recall later.
 *
 * @param msg Error message to be set.
 */
void el_error_msg_set(const char *msg) {
	/* Make sure we have enough space to store our error message. */
	el_error_msg_buf = (char *)realloc(el_error_msg_buf,
									   (strlen(msg) + 1) * sizeof(char));

	/* Copy the error message. */
	strcpy(el_error_msg_buf, msg);
}

/**
 * Sets the error message that the user can later recall using a syntax akin to
 * printf.
 *
 * @param format Format string just like in printf.
 * @param ...    Things to place inside the formatted string.
 */
void el_error_msg_format(const char *format, ...) {
	size_t len;
	va_list args;

	/* Allocate enough space for our message. */
#ifndef vsnprintf
	len = VSNPRINTF_MAX_LEN;
#else
	va_start(args, format);
	len = vsnprintf(NULL, 0, format, args) + 1;
	va_end(args);
#endif /* vsnprintf */
	el_error_msg_buf = (char *)realloc(el_error_msg_buf,
									   len * sizeof(char));

	/* Copy our formatted message. */
	va_start(args, format);
	vsprintf(el_error_msg_buf, format, args);
	va_end(args);

#ifndef vsnprintf
	/* Ensure that we have a properly sized string. */
	len = strlen(el_error_msg_buf) + 1;
	el_error_msg_buf = (char *)realloc(el_error_msg_buf,
									   len * sizeof(char));
#endif /* vsnprintf */
}

/**
 * Gets the last error message string.
 *
 * @return A read-only pointer to the internal last error message string buffer.
 */
const char *el_error_msg(void) {
	return (const char *)el_error_msg_buf;
}

/**
 * Prints the last error message thrown by the library to STDERR.
 */
void el_error_print(void) {
	fprintf(stderr, "ERROR: %s\n", el_error_msg_buf);
}

/**
 * Frees up the internal error message string buffer.
 */
void el_error_free(void) {
	/* Do we even have anything to free? */
	if (el_error_msg_buf == NULL)
		return;

	/* Free up the error message. */
	free(el_error_msg_buf);
	el_error_msg_buf = NULL;
}

/**
 * Copies one string to another using a start and end portions of an original
 * string. Basically strcpy that accepts substrings. It will always NULL
 * terminate the destination.
 *
 * @warning This function will allocate memory for dest. Make sure you free this
 *          string later.
 *
 * @param dest  Destination string. (Will be allocated by this function.)
 * @param start Start of the string to be copied.
 * @param end   Where should we stop copying the string? (Inclusive)
 *
 * @return Number of bytes copied.
 */
size_t el_util_strstrcpy(char **dest, const char *start, const char *end) {
	size_t len;
	char *dest_buf;
	const char *src_buf;

	/* Allocate space for the new string. */
	len = (end - start) + 1;
	*dest = (char *)malloc(len * sizeof(char));

	/* Copy the new string over. */
	dest_buf = *dest;
	src_buf = start;
	len = 0;
	do {
		*dest_buf = *src_buf;

		src_buf++;
		dest_buf++;
		len++;
	} while (src_buf <= end);
	*dest_buf = '\0';

	return len;
}

/**
 * Similar to strcpy except we allocate (reallocate if needed) the destination
 * string automatically.
 *
 * @warning This function will allocate memory for dest. Make sure you free this
 *          string later.
 *
 * @param dest Destination string. (Will be [re]allocated by this function.)
 * @param src  Source string to be copied.
 *
 * @return Number of bytes copied.
 */
size_t el_util_strcpy(char **dest, const char *src) {
	size_t len;
	char *dest_buf;
	const char *src_buf;

	/* Check if we have a valid destination pointer. */
	if (dest == NULL)
		return 0;

	/* Allocate space for the new string. */
	len = strlen(src);
	*dest = (char *)realloc(*dest, (len + 1) * sizeof(char));

	/* Copy the new string over. */
	dest_buf = *dest;
	src_buf = src;
	do {
		*dest_buf = *src_buf;

		src_buf++;
		dest_buf++;
	} while (*src_buf != '\0');

	return len;
}

/**
 * Checks if a file exists in the file system.
 *
 * @param fname File path to be checked.
 *
 * @return Does this file exist?
 */
bool el_util_file_exists(const char *fname) {
	return access(fname, F_OK) == 0;
}
