/**
 * @file argpus.h
 * @author Daniel Starke
 * @copyright Copyright 2017-2018 Daniel Starke
 * @see argp.h
 * @see ARGP_FUNC()
 * @date 2017-05-22
 * @version 2018-12-13
 */
#ifndef __LIBPCF_ARGPUS_H__
#define __LIBPCF_ARGPUS_H__

#include <wchar.h>
#include "target.h"
#include "argp.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Defines the type of a single long option list element.
 */
typedef struct tArgPEUS {
	const wchar_t * name;
	int has_arg;
	int * flag;
	int val;
} tArgPEUS;


/**
 * Defines the argument parser context.
 * All fields are expected to be set 0 on the initial state, except flags, shortOpts and longOpts.
 */
typedef struct tArgPUS {
	int i; /**< next argument list index (may be changed by user) */
	int nextI; /**< argument list index to next character to be parsed */
	int lastOpt; /**< index to the previous option (for reordering) */
	int opt; /**< erroneous option */
	tArgPFlag flags; /**< see tArgPFlag */
	const wchar_t * arg; /**< next argument to be parsed */
	const wchar_t * next; /**< next character to be parsed */
	const wchar_t * shortOpts; /**< short options */
	const tArgPEUS * longOpts; /**< long option list */
	int longMatch; /**< long option list index on match, else <0 */
	tArgPState state; /**< internal argument parser state (starts with 0) */
} tArgPUS;


/** @copydoc ARGP_FUNC() */
int argpus_parse(tArgPUS * o, int argc, wchar_t * const * argv);


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_ARGPUS_H__ */
