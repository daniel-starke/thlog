/**
 * @file parse.h
 * @author Daniel Starke
 * @copyright Copyright 2019 Daniel Starke
 * @date 2019-06-03
 * @version 2019-06-22
 */
#ifndef __PARSE_H__
#define __PARSE_H__


/** Float parser states. */
typedef enum tPFloatState {
	PFS_START = 0,
	PFS_STOP,
	PFS_ERROR_TOKEN,
	PFS_ERROR_OVERFLOW,
	PFS_INTEGRAL,
	PFS_FRACTION,
	PFS_REMAINING
} tPFloatState;


/** Error number parser states. */
typedef enum tPErrState {
	PES_START = 0,
	PES_STOP,
	PES_ERROR_TOKEN,
	PES_ERROR_OVERFLOW,
	PES_E__,
	PES_ER_,
	PES_ERR,
	PES_INTEGRAL_START,
	PES_INTEGRAL
} tPErrState;


/** Output format string parser states. */
typedef enum tPFmtState {
	PFMTS_START = 0,
	PFMTS_STOP,
	PFMTS_ERROR_TOKEN,
	PFMTS_ERROR_OVERFLOW,
	PFMTS_FLAG,
	PFMTS_WIDTH,
	PFMTS_PRECISION_START,
	PFMTS_PRECISION,
	PFMTS_TYPE,
	PFMTS_SUBTYPE
} tPFmtState;


/** Output format string parser states. */
typedef enum tPFmtFlag {
	PFMTF_LEFT_ALIGN = 0x01,
	PFMTF_SIGN       = 0x02,
	PFMTF_ZERO       = 0x04,
	PFMTF_BLANK      = 0x08,
	PFMTF_HASHTAG    = 0x10
} tPFmtFlag;


/** Float parser context. */
typedef struct tPFloatCtx {
	tPFloatState state;
	float result;
	float sign;
	unsigned long integral;
	unsigned long fraction;
	unsigned int digits;
} tPFloatCtx;


/** Error number parser context. */
typedef struct tPErrCtx {
	tPErrState state;
	unsigned int result;
} tPErrCtx;


/** Output format string parser context. */
typedef struct tPFmtCtx {
	tPFmtState state;
	tPFmtFlag flags;
	unsigned int width;
	unsigned int precision;
	int type;
	int subType;
} tPFmtCtx;


int parseFloat(tPFloatCtx * ctx, const int c);
int parseErr(tPErrCtx * ctx, const int c);
int parseFmt(tPFmtCtx * ctx, const int c);


#endif /* __PARSE_H__ */
