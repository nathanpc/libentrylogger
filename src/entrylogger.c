/**
 * entrylogger.c
 * A library for parsing and easily working with EntryLogger documents.
 *
 * @author Nathan Campos <nathan@innoveworkshop.com>
 */

#include "entrylogger.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 * @param fname Document file path.
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

	/* Allocate space for the filename and copy it over. */
	doc->fname = (char *)realloc(doc->fname,
								 (strlen(fname) + 1) * sizeof(char));
	strcpy(doc->fname, fname);

	/* Set the file opening mode. */
	strncpy(doc->fmode, fmode, 2);

	/* Finally open the file. */
	doc->fh = fopen(fname, fmode);
	if (doc->fh == NULL) {
		el_error_msg_format(EMSG("Couldn't open file \"%s\": %s."), fname,
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
 * Parses a document's header.
 *
 * @param doc Opened document handle.
 *
 * @return EL_OK if the operation was successful.
 *         EL_ERROR_FILE if something happened while trying to read the file.
 */
el_err_t el_doc_parse_header(eld_handle_t *doc) {
	return EL_ERROR_NOT_IMPL;
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
