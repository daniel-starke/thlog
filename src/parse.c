/**
 * @file parse.c
 * @author Daniel Starke
 * @copyright Copyright 2019 Daniel Starke
 * @date 2019-06-03
 * @version 2023-04-10
 */
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include "parse.h"


/**
 * Passive float parser. Supports sign, integral and fraction part separated by a dot. No other
 * forms are supported. Initialize ctx to zero (e.g. with memset) before calling this function the
 * first time. PFS_STOP is returned if the character provided signaled the end of the float. The
 * float can be retrieved from ctx->result.
 *
 * @param[in,out] ctx - float parser context
 * @param[in] c - character to process
 * @return  1 - success
 * @return  0 - failed (check ctx->state for more details)
 * @return -1 - error
 */
int parseFloat(tPFloatCtx * ctx, const int c) {
	if (ctx == NULL) return -1;
	switch (ctx->state) {
	case PFS_START:
		ctx->result = nanf("");
		ctx->integral = 0;
		ctx->fraction = 0;
		ctx->digits = 0;
		if (c == '-') {
			ctx->state = PFS_INTEGRAL;
			ctx->sign = -1.0f;
		} else if (c >= '0' && c <= '9') {
			ctx->state = PFS_INTEGRAL;
			ctx->sign = 1.0f;
			goto onIntegral;
		} else if (c == '.') {
			ctx->state = PFS_FRACTION;
		} else {
			ctx->state = PFS_ERROR_TOKEN;
			return 0;
		}
		break;
	case PFS_STOP:
	case PFS_ERROR_TOKEN:
	case PFS_ERROR_OVERFLOW:
		return 0;
	case PFS_INTEGRAL:
onIntegral:
		if (c >= '0' && c <= '9') {
			const unsigned long old = ctx->integral;
			ctx->integral = (ctx->integral * 10) + (unsigned long)(c - '0');
			if (ctx->integral < old) {
				ctx->state = PFS_ERROR_OVERFLOW;
				return 0;
			}
		} else if (c == '.') {
			ctx->state = PFS_FRACTION;
		} else {
			ctx->state = PFS_STOP;
			ctx->result = ctx->sign * (float)(ctx->integral);
			return 0;
		}
		break;
	case PFS_FRACTION:
		if (c >= '0' && c <= '9') {
			const unsigned long old = ctx->fraction;
			ctx->fraction = (ctx->fraction * 10) + (unsigned long)(c - '0');
			if (ctx->fraction < old) {
				ctx->state = PFS_REMAINING;
				ctx->fraction = old;
			} else {
				ctx->digits++;
			}
		} else {
			goto onEnd;
		}
		break;
	case PFS_REMAINING:
		if (c < '0' || c > '9') {
onEnd:
			ctx->state = PFS_STOP;
			ctx->result = ctx->sign * (
				(float)(ctx->integral) + ((float)(ctx->fraction) / powf(10.0f, (float)(ctx->digits)))
			);
			return 0;
		}
		break;
	}
	return 1;
}


/**
 * Passive error number parser. Supports error numbers in the format "Err:#" with # as an unsigned
 * integer. No other forms are supported. The parser is case sensitive. Initialize ctx to zero
 * (e.g. with memset) before calling this function the first time. PES_STOP is returned if the
 * character provided signaled the end of the number. The number can be retrieved from ctx->result.
 *
 * @param[in,out] ctx - err parser context
 * @param[in] c - character to process
 * @return  1 - success
 * @return  0 - failed (check ctx->state for more details)
 * @return -1 - error
 */
int parseErr(tPErrCtx * ctx, const int c) {
	if (ctx == NULL) return -1;
	switch (ctx->state) {
	case PES_START:
		ctx->result = 0;
		if (c == 'E') {
			ctx->state = PES_E__;
		} else {
			ctx->state = PES_ERROR_TOKEN;
			return 0;
		}
		break;
	case PES_STOP:
	case PES_ERROR_TOKEN:
	case PES_ERROR_OVERFLOW:
		return 0;
	case PES_E__:
		if (c == 'r') {
			ctx->state = PES_ER_;
		} else {
			ctx->state = PES_ERROR_TOKEN;
			return 0;
		}
		break;
	case PES_ER_:
		if (c == 'r') {
			ctx->state = PES_ERR;
		} else {
			ctx->state = PES_ERROR_TOKEN;
			return 0;
		}
		break;
	case PES_ERR:
		if (c == ':') {
			ctx->state = PES_INTEGRAL_START;
		} else {
			ctx->state = PES_ERROR_TOKEN;
			return 0;
		}
		break;
	case PES_INTEGRAL_START:
		if (c >= '0' && c <= '9') {
			ctx->state = PES_INTEGRAL;
			ctx->result = (unsigned int)(c - '0');
		} else if (c != ' ' && c != '\t') {
			ctx->state = PES_ERROR_TOKEN;
			return 0;
		}
		break;
	case PES_INTEGRAL:
		if (c >= '0' && c <= '9') {
			const unsigned int old = ctx->result;
			ctx->result = (ctx->result * 10) + (unsigned int)(c - '0');
			if (ctx->result < old) {
				ctx->state = PES_ERROR_OVERFLOW;
				ctx->result = UINT_MAX;
				return 0;
			}
		} else {
			ctx->state = PES_STOP;
			return 0;
		}
		break;
	}
	return 1;
}


/**
 * Passive output format string parser. Initialize ctx to zero (e.g. with memset) before calling
 * this function the first time. PFMTS_STOP is returned if the character provided signaled the end
 * of the format code. The type and modifiers can be retrieved from ctx->flags, ctx->width,
 * ctx->precision, ctx->type and ctx->subType.
 *
 * @param[in,out] ctx - output format string parser context
 * @param[in] c - character to process
 * @return  1 - success
 * @return  0 - failed (check ctx->state for more details)
 * @return -1 - error
 * @remarks Expects the format %[flags][width][.precision]type[subtype]
 */
int parseFmt(tPFmtCtx * ctx, const int c) {
	if (ctx == NULL) return -1;
	switch (ctx->state) {
	case PFMTS_START:
		if (c == '%') {
			ctx->state = PFMTS_FLAG;
			ctx->flags = (tPFmtFlag)0;
			ctx->width = 0;
			ctx->precision = 0;
			ctx->type = 0;
			ctx->subType = 0;
		} else {
			ctx->state = PFMTS_ERROR_TOKEN;
			return 0;
		}
		break;
	case PFMTS_STOP:
	case PFMTS_ERROR_TOKEN:
	case PFMTS_ERROR_OVERFLOW:
		return 0;
	case PFMTS_FLAG:
		switch (c) {
		case '-': ctx->flags = (tPFmtFlag)(ctx->flags | PFMTF_LEFT_ALIGN); break;
		case '+': ctx->flags = (tPFmtFlag)(ctx->flags | PFMTF_SIGN); break;
		case '0': ctx->flags = (tPFmtFlag)(ctx->flags | PFMTF_ZERO); break;
		case ' ': ctx->flags = (tPFmtFlag)(ctx->flags | PFMTF_BLANK); break;
		case '#': ctx->flags = (tPFmtFlag)(ctx->flags | PFMTF_HASHTAG); break;
		case '.': ctx->state = PFMTS_PRECISION_START; break;
		default:
			if (c >= '1' && c <= '9') {
				ctx->state = PFMTS_WIDTH;
				ctx->width = (unsigned int)(c - '0');
			} else {
				ctx->state = PFMTS_TYPE;
				goto onType;
			}
			break;
		}
		break;
	case PFMTS_WIDTH:
		if (c >= '0' && c <= '9') {
			const unsigned int old = ctx->width;
			ctx->width = (ctx->width * 10) + (unsigned int)(c - '0');
			if (ctx->width < old) {
				ctx->state = PFMTS_ERROR_OVERFLOW;
				ctx->width = UINT_MAX;
				return 0;
			}
		} else if (c == '.') {
			ctx->state = PFMTS_PRECISION_START;
		} else {
			ctx->state = PFMTS_TYPE;
			goto onType;
		}
		break;
	case PFMTS_PRECISION_START:
		if (c >= '0' && c <= '9') {
			ctx->state = PFMTS_PRECISION;
			ctx->precision = (unsigned int)(c - '0');
		} else {
			ctx->state = PFMTS_TYPE;
			goto onType;
		}
		break;
	case PFMTS_PRECISION:
		if (c >= '0' && c <= '9') {
			const unsigned int old = ctx->precision;
			ctx->precision = (ctx->precision * 10) + (unsigned int)(c - '0');
			if (ctx->precision < old) {
				ctx->state = PFMTS_ERROR_OVERFLOW;
				ctx->precision = UINT_MAX;
				return 0;
			}
		} else {
			ctx->state = PFMTS_TYPE;
			goto onType;
		}
		break;
	case PFMTS_TYPE:
onType:
		switch (c) {
		case '%':
		case 'a':
		case 'A':
		case 'b':
		case 'B':
		case 'c':
		case 'C':
		case 'd':
		case 'D':
		case 'e':
		case 'E':
		case 'F':
		case 'g':
		case 'G':
		case 'h':
		case 'H':
		case 'I':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'M':
		case 'n':
		case 'O':
		case 'p':
		case 'P':
		case 'r':
		case 'R':
		case 's':
		case 'S':
		case 't':
		case 'T':
		case 'u':
		case 'U':
		case 'V':
		case 'w':
		case 'W':
		case 'x':
		case 'X':
		case 'y':
		case 'Y':
		case 'z':
		case 'Z':
			ctx->state = PFMTS_STOP;
			ctx->type = c;
			return 0;
		case 'v':
			ctx->state = PFMTS_SUBTYPE;
			ctx->type = c;
			break;
		default:
			ctx->state = PFMTS_ERROR_TOKEN;
			ctx->type = c;
			return 0;
		}
		break;
	case PFMTS_SUBTYPE:
		switch (c) {
		case 'C':
		case 'F':
		case 'H':
			ctx->state = PFMTS_STOP;
			ctx->subType = c;
			return 0;
		default:
			ctx->state = PFMTS_ERROR_TOKEN;
			ctx->subType = c;
			return 0;
		}
		break;
	}
	return 1;
}
