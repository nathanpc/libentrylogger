/**
 * bcshim.h
 * Shim definitions to compile the project using Borland C++ 3.
 *
 * @author Nathan Campos <nathan@innoveworkshop.com>
 */

#ifndef _BCSHIM_H
#define _BCSHIM_H

/* Integer types. */
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;

/* Boolean. */
typedef uint8_t bool;
#define true 1
#define false 0

#endif /* _BCSHIM_H */