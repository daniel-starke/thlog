/**
 * @file thlog.c
 * @author Daniel Starke
 * @copyright Copyright 2019 Daniel Starke
 * @date 2019-06-02
 * @version 2023-04-11
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utility/cvutf8.h"
#include "utility/getopt.h"
#include "utility/mingw-unicode.h"
#include "utility/serial.h"
#include "utility/target.h"
#include "utility/tchar.h"
#include "license.i"
#include "parse.h"
#include "version.h"


#if defined(PCF_IS_WIN)
#include <windows.h>
#elif defined(PCF_IS_LINUX)
#include <unistd.h>
#else /* not PCF_IS_WIN and not PCF_IS_LINUX */
#error Unsupported target OS.
#endif


/** Defines the default format string. */
#define DEFAULT_FORMAT _T("%Y-%m-%d %H:%M:%S\\t%.1vC\\t%.1vH\\n")


/** Defines the default update interval in seconds. */
#define DEFAULT_INTERVAL 10


/** Timeout resolution in milliseconds. Needed to handle SIGINT/SIGTERM quickly. */
#define TIMEOUT_RESOLUTION 100


/** Defines the input buffer size for the sensor data. */
#define INPUT_BUFFER_SIZE 64


/** Returns the given UTF-8 error message string. */
#define MSGU(x) ((const char *)fmsg[(x)])


/** Returns the given native (TCHAR) error message string. */
#define MSGT(x) ((const TCHAR *)fmsg[(x)])


/** Defines the data processing configuration parameters. */
typedef struct tConfig {
	size_t intvl; /**< update interval in seconds */
	int utc; /**< non-zero to represent the time in UTC instead of local time */
	const TCHAR * fmt; /**< output format string supporting all of strftime and %vC, %vF, %vH */
} tConfig;


typedef enum {
	MSGT_SUCCESS = 0,
	MSGT_ERR_NO_MEM,
	MSGT_ERR_OPT_NO_ARG,
	MSGT_ERR_OPT_BAD_INTERVAL,
	MSGT_ERR_OPT_NO_DEVICE,
	MSGT_ERR_OPT_AMB_C,
	MSGT_ERR_OPT_AMB_S,
	MSGT_ERR_OPT_AMB_X,
	MSGT_ERR_REMOTE_CONNECT,
	MSGT_ERR_REMOTE_READ,
	MSGT_ERR_REMOTE_VALUE,
	MSGT_ERR_REMOTE_CHECKSUM,
	MSGT_ERR_FMT_OVERFLOW,
	MSGT_ERR_FMT_SYNTAX,
	MSGT_ERR_FMT_LENGTH,
	MSGT_ERR_FMT_API,
	MSGT_ERR_FMT_WRITE,
	MSGT_INFO_SIGTERM,
	MSG_COUNT
} tMessage;


static const void * fmsg[MSG_COUNT] = {
	/* MSGT_SUCCESS                    */ _T(""), /* never used for output */
	/* MSGT_ERR_NO_MEM                 */ _T("Error: Failed to allocate memory.\n"),
	/* MSGT_ERR_OPT_NO_ARG             */ _T("Error: Option argument is missing for '%s'.\n"),
	/* MSGT_ERR_OPT_BAD_INTERVAL       */ _T("Error: Invalid interval value. (%s)"),
	/* MSGT_ERR_OPT_NO_DEVICE          */ _T("Error: Missing device.\n"),
	/* MSGT_ERR_OPT_AMB_C              */ _T("Error: Unknown or ambiguous option '-%c'.\n"),
	/* MSGT_ERR_OPT_AMB_S              */ _T("Error: Unknown or ambiguous option '%s'.\n"),
	/* MSGT_ERR_OPT_AMB_X              */ _T("Error: Unknown option character '0x%02X'.\n"),
	/* MSGT_ERR_REMOTE_CONNECT         */ _T2("Error: Failed to connect to remote device via %" PRUTF8 ".\n"),
	/* MSGT_ERR_REMOTE_READ            */ _T("Error: Failed to read data from remote device.\n"),
	/* MSGT_ERR_REMOTE_VALUE           */ _T("Error: The remote device returned error code %u.\n"),
	/* MSGT_ERR_REMOTE_CHECKSUM        */ _T("Error: Checksum of the remote data failed.\n"),
	/* MSGT_ERR_FMT_OVERFLOW           */ _T("Error: Format code width/precision modifier is too large.\n%.*s<<<HERE<<<%s\n"),
	/* MSGT_ERR_FMT_SYNTAX             */ _T("Error: Format code syntax error.\n%.*s<<<HERE<<<%s\n"),
	/* MSGT_ERR_FMT_LENGTH             */ _T("Error: Format code length error.\n%.*s<<<HERE<<<%s\n"),
	/* MSGT_ERR_FMT_API                */ _T("Error: Format code error reported by underlaying API.\n%.*s<<<HERE<<<%s\n"),
	/* MSGT_ERR_FMT_WRITE              */ _T("Error: Failed to write formatted sensor data.\n"),
	/* MSGT_INFO_SIGTERM               */ _T("Info: Received signal. Finishing current operation.\n")
};


static volatile int signalReceived = 0;
static int verbose = 1; /* 0 = critical, 1 = error, 2 = warn, 3 = info, 4 = debug */
static FILE * fin = NULL;
static FILE * fout = NULL;
static FILE * ferr = NULL;


/* forward declarations */
static void printHelp(void);
static void handleSignal(int signum);
static int processData(tSerial * ser, uint8_t * inBuf, const size_t inBufSize, const tConfig * cfg);
static int printData(FILE * fd, const TCHAR * fmt, const struct tm * timeInfo, const float temp, const float rh);
static void delay(const unsigned long duration);


/**
 * Main entry point.
 */
int _tmain(int argc, TCHAR ** argv) {
	enum {
		GETOPT_LICENSE = 1,
		GETOPT_UTF8 = 2,
		GETOPT_VERSION = 3
	};
	char POSIXLY_CORRECT[] = "POSIXLY_CORRECT=";
	int ret = EXIT_FAILURE;
	TCHAR * strNum;
	struct option longOptions[] = {
		{_T("license"),      no_argument,       NULL, GETOPT_LICENSE},
		{_T("utf8"),         no_argument,       NULL,    GETOPT_UTF8},
		{_T("version"),      no_argument,       NULL, GETOPT_VERSION},
		{_T("format"),       required_argument, NULL,        _T('f')},
		{_T("help"),         no_argument,       NULL,        _T('h')},
		{_T("interval"),     required_argument, NULL,        _T('i')},
		{_T("utc"),          no_argument,       NULL,        _T('u')},
		{_T("verbose"),      no_argument,       NULL,        _T('v')},
		{NULL, 0, NULL, 0}
	};
	char * device = NULL;
	uint8_t * inBuf = NULL;
	tSerial * remoteConn = NULL;
	tConfig config = {
		DEFAULT_INTERVAL, /* update interval in seconds */
		0, /* use local time */
		DEFAULT_FORMAT /* format string */
	};

	/* ensure that the environment does not change the argument parser behavior */
	putenv(POSIXLY_CORRECT);

	/* set the file descriptors */
	fin  = stdin;
	fout = stdout;
	ferr = stderr;

#if defined(UNICODE) && defined(PCF_IS_WIN) && !defined(__CYGWIN__)
	_setmode(_fileno(fout), _O_U16TEXT);
	_setmode(_fileno(ferr), _O_U16TEXT);
#endif /* PCF_IS_WIN */

	if (argc < 2) {
		printHelp();
		return EXIT_FAILURE;
	}

	while (1) {
		const int res = getopt_long(argc, argv, _T(":f:hi:uv"), longOptions, NULL);

		if (res == -1) break;
		switch (res) {
		case GETOPT_LICENSE:
#ifdef UNICODE
			{
				TCHAR * text = _tfromUtf8N(licenseText, sizeof(licenseText));
				if (text == NULL) {
					_ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
					goto onError;
				}
				_ftprintf(ferr, _T("%s"), text);
				if (text != (const TCHAR *)licenseText) free(text);
			}
#else /* !UNICODE */
			_ftprintf(ferr, _T("%s"), licenseText);
#endif /* UNICODE */
			signalReceived++;
			return EXIT_SUCCESS;
			break;
		case GETOPT_UTF8:
#ifdef UNICODE
			_setmode(_fileno(fout), _O_U8TEXT);
			_setmode(_fileno(ferr), _O_U8TEXT);
#endif /* UNICODE */
			break;
		case GETOPT_VERSION:
			_ftprintf(fout, _T("%u.%u.%u"), PROGRAM_VERSION);
			signalReceived++;
			return EXIT_SUCCESS;
			break;
		case _T('f'):
			config.fmt = optarg;
			break;
		case _T('h'):
			printHelp();
			signalReceived++;
			return EXIT_SUCCESS;
			break;
		case _T('i'):
			config.intvl = (size_t)_tcstol(optarg, &strNum, 10);
			if (config.intvl < 1 || strNum == NULL || *strNum != 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_BAD_INTERVAL), optarg);
				goto onError;
			}
			break;
		case _T('u'):
			config.utc = 1;
			break;
		case _T('v'):
			verbose++;
			break;
		case _T(':'):
			_ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_ARG), argv[optind - 1]);
			goto onError;
			break;
		case _T('?'):
			if (_istprint((TINT)optopt) != 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_AMB_C), optopt);
			} else if (optopt == 0) {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_AMB_S), argv[optind - 1]);
			} else {
				_ftprintf(ferr, MSGT(MSGT_ERR_OPT_AMB_X), (int)optopt);
			}
			goto onError;
			break;
		default:
			abort();
		}
	}

	if (optind >= argc) {
		_ftprintf(ferr, MSGT(MSGT_ERR_OPT_NO_DEVICE));
		goto onError;
	}
	device = _ttoUtf8(argv[optind]);
	if (device == NULL) {
		_ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}

	/* allocate input buffer */
	inBuf = malloc(INPUT_BUFFER_SIZE);
	if (inBuf == NULL) {
		_ftprintf(ferr, MSGT(MSGT_ERR_NO_MEM));
		goto onError;
	}

	/* install signal handlers */
	signal(SIGINT, handleSignal);
	signal(SIGTERM, handleSignal);

	/* connect to remote device */
	remoteConn = ser_create(device, 9600, SFR_8N1, SFC_NONE);
	if (remoteConn == NULL) {
		_ftprintf(ferr, MSGT(MSGT_ERR_REMOTE_CONNECT), device);
		goto onError;
	}

	/* wait for remote device to connect */
	delay(1000);
	ser_clear(remoteConn);

	/* read data and output results once per interval */
	ret = processData(remoteConn, inBuf, INPUT_BUFFER_SIZE, &config);
onError:
	if (ret != EXIT_SUCCESS) signalReceived++;
	if (remoteConn != NULL) ser_delete(remoteConn);
	if (inBuf != NULL) free(inBuf);
	if (device != NULL) free(device);
	return ret;
};


/**
 * Write the help for this application to standard out.
 */
static void printHelp(void) {
	_ftprintf(ferr,
	_T("thlog [options] <device>\n")
	_T("\n")
	_T("-f, --format <string>\n")
	_T("      Defines the output format string. The allows the same as strftime in C\n")
	_T("      and the following:\n")
	_T("      %%vC - temperature in degrees Celsius\n")
	_T("      %%vF - temperature in degrees Fahrenheit\n")
	_T("      %%vH - relative humidity in percent\n")
	_T("      The format modifiers of printf's %%f can be applied here.\n")
	_T("      The default is \"%s\"\n")
	_T("-h, --help\n")
	_T("      Print short usage instruction.\n")
	_T("-i, --interval <number>\n")
	_T("      Update interval in seconds. Default: %u\n")
	_T("    --license\n")
	_T("      Displays the licenses for this program.\n")
#ifdef UNICODE
	_T("    --utf8\n")
	_T("      Sets the encoding for error console to UTF-8.\n")
	_T("      The default is UTF-16.\n")
#endif /* UNICODE */
	_T("-u\n")
	_T("      Display time in UTC instead of local time.\n")
	_T("-v\n")
	_T("      Increases verbosity.\n")
	_T("    --version\n")
	_T("      Outputs the program version.\n")
	_T("\n")
	_T("thlog %u.%u.%u\n")
	_T("https://github.com/daniel-starke/thlog\n")
	, DEFAULT_FORMAT
	, DEFAULT_INTERVAL
	, PROGRAM_VERSION);
}


/**
 * Handles external signals.
 *
 * @param[in] signum - received signal number
 */
static void handleSignal(int signum) {
	PCF_UNUSED(signum)
	if (signalReceived == 0 && verbose > 2) _ftprintf(ferr, MSGT(MSGT_INFO_SIGTERM));
	signalReceived++;
}


/**
 * Processes the data from the serial device connected via ser. This function outputs the received
 * data with the format string provided on fout.
 *
 * @param[in,out] ser - serial device handle
 * @param[in,out] inBuf - input buffer to use
 * @param[in] inBufSize - size of inBuf in bytes
 * @param[in] cfg - data processing configuration
 * @return program exit code
 * @remarks Possible error codes are defined in arduino/DHT11.hpp and arduino/DHT22.hpp.
 */
static int processData(tSerial * ser, uint8_t * inBuf, const size_t inBufSize, const tConfig * cfg) {
	if (signalReceived == 0 && (ser == NULL || inBuf == NULL || inBufSize == 0 || cfg == NULL)) return EXIT_FAILURE;
	enum {
		ST_TEMP,
		ST_RH,
		ST_SUM
	} pState;
	float temp, rh, sum;
	float tempSum, rhSum;
	size_t valueCount;
	tPFloatCtx pFloat;
	tPErrCtx pErr;
	time_t intvlStart, now;

	pState = ST_TEMP;
	temp = 0.0f;
	rh = 0.0f;
	tempSum = 0.0f;
	rhSum = 0.0f;
	valueCount = 0;
	memset(&pFloat, 0, sizeof(pFloat));
	memset(&pErr, 0, sizeof(pErr));
	time(&intvlStart);

	while (signalReceived == 0) {
		errno = 0;
		const ssize_t res = ser_read(ser, inBuf, inBufSize, TIMEOUT_RESOLUTION);
		if (res == -1) {
			if (verbose > 0 && errno != EINTR) _ftprintf(ferr, MSGT(MSGT_ERR_REMOTE_READ));
			signalReceived++;
			return EXIT_FAILURE;
		}
		for (ssize_t i = 0; i < res; i++) {
			const int c = (int)(inBuf[i]);
			/* check if an error was returned */
			parseErr(&pErr, c);
			switch (pErr.state) {
			case PES_STOP:
				if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_REMOTE_VALUE), pErr.result);
				/* fall-through */
			case PES_ERROR_TOKEN:
			case PES_ERROR_OVERFLOW:
				memset(&pErr, 0, sizeof(pErr));
				break;
			default:
				break;
			}
			/* check if a value was parsed */
			int pOk = parseFloat(&pFloat, c);
			switch (pFloat.state) {
			case PFS_STOP:
				switch (pState) {
				case ST_TEMP:
					temp = pFloat.result;
					pState = ST_RH;
					break;
				case ST_RH:
					rh = pFloat.result;
					pState = ST_SUM;
					break;
				case ST_SUM:
					sum = pFloat.result;
					pState = ST_TEMP;
					if (fabsf(temp + rh - sum) > 0.001f) {
						if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_REMOTE_CHECKSUM));
						break;
					}
					tempSum += temp;
					rhSum += rh;
					valueCount++;
					break;
				}
				pOk = 1;
				memset(&pFloat, 0, sizeof(pFloat));
				memset(&pErr, 0, sizeof(pErr));
				break;
			case PFS_ERROR_TOKEN:
			case PFS_ERROR_OVERFLOW:
				pOk = 0; /* re-initialize value parser */
				break;
			default:
				break;
			}
			/* re-initialize value parser */
			if (pOk == 0) {
				memset(&pFloat, 0, sizeof(pFloat));
				pState = ST_TEMP;
			}
			/* re-initialize all parsers on line end */
			if (c == '\r' || c == '\n') {
				memset(&pFloat, 0, sizeof(pFloat));
				memset(&pErr, 0, sizeof(pErr));
				pState = ST_TEMP;
			}
		}
		/* check update interval */
		time(&now);
		if (valueCount == 0) {
			intvlStart = now;
			continue;
		}
		if (difftime(now, intvlStart) >= (double)(cfg->intvl)) {
			/* output values */
			struct tm * timeInfo = (cfg->utc != 0) ? gmtime(&now) : localtime(&now);
			if (printData(fout, cfg->fmt, timeInfo, tempSum / (float)valueCount, rhSum / (float)valueCount) < 0) {
				if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_WRITE));
				return EXIT_FAILURE;
			}
			fflush(fout);
			/* reset values for next interval */
			tempSum = 0.0f;
			rhSum = 0.0f;
			valueCount = 0;
			intvlStart = now;
		}
	}
	return EXIT_SUCCESS;
}


/**
 * Outputs the given sensor data.
 *
 * @param[in,out] fd - file descriptor to write to
 * @param[in] fmt - format string
 * @param[in] timeInfo - time reference
 * @param[in] temp - temperature value in degrees Celsius
 * @param[in] rh - relative humidity in percent
 * @return number of bytes written or -1 on error
 * @remarks see strftime() for valid time related format codes
 */
static int printData(FILE * fd, const TCHAR * fmt, const struct tm * timeInfo, const float temp, const float rh) {
	static const TCHAR * stateStr[] = {
		_T("START"),
		_T("STOP"),
		_T("ERROR_TOKEN"),
		_T("ERROR_OVERFLOW"),
		_T("FLAG"),
		_T("WIDTH"),
		_T("PRECISION_START"),
		_T("PRECISION"),
		_T("TYPE"),
		_T("SUBTYPE")
	};
	if (fd == NULL || fmt == NULL || timeInfo == NULL) return -1;
	int written, res;
	TCHAR tFmt[4];
	TCHAR buf[64];
	const TCHAR * start, * ptr;
	int last, esc;
	tPFmtCtx pFmt;

	res = 0;
	start = fmt;
	last = 0;
	esc = 0;
	memset(&pFmt, 0, sizeof(pFmt));

	for (ptr = fmt; *ptr != 0; ptr++) {
		const int c = *ptr;
		if (verbose > 3) _ftprintf(ferr, _T("CHAR:%i '%c', STATE:%s, ESC:%i\n"), c, c, stateStr[pFmt.state], esc);
		/* process character escape codes */
		if (esc != 0) {
			last = c;
			esc = 0;
			switch (c) {
			case '\\': esc = '\\'; break;
			case 'a': esc = 0x07; break;
			case 'b': esc = 0x08; break;
			case 'e': esc = 0x1B; break;
			case 'f': esc = 0x0C; break;
			case 'n': esc = 0x0A; break;
			case 'r': esc = 0x0D; break;
			case 't': esc = 0x09; break;
			default: break;
			}
			if (esc != 0) {
				written = _ftprintf(fd, _T("%c"), esc);
				if (written < 0) return -1;
				res += written;
				start = ptr + 1;
				esc = 0;
				continue;
			}
		}
		/* process codes and literals */
		if (pFmt.state == PFMTS_START) {
			if (c == '%') {
				/* format code start */
				if (start != ptr) {
					/* print string literal (i.e. string part without any format codes) */
					written = _ftprintf(fd, _T("%.*s"), (int)(ptr - start), start);
					if (written < 0) return -1;
					res += written;
					start = ptr;
				}
			} else if (c == '\\' && esc == 0) {
				/* escape code start */
				if (start != ptr) {
					/* print string literal (i.e. string part without any format codes) */
					written = _ftprintf(fd, _T("%.*s"), (int)(ptr - start), start);
					if (written < 0) return -1;
					res += written;
					start = ptr;
				}
				esc = 1;
				continue;
			} else {
				/* string literal part */
				last = c;
				continue;
			}
		}
		/* process format codes */
		if (parseFmt(&pFmt, c) != 1 && pFmt.state != PFMTS_STOP) {
			/* invalid format code syntax */
			if (verbose > 0) {
				if (pFmt.state == PFMTS_ERROR_OVERFLOW) {
					_ftprintf(ferr, MSGT(MSGT_ERR_FMT_OVERFLOW), (int)(ptr + 1 - fmt), fmt, ptr + 1);
				} else {
					_ftprintf(ferr, MSGT(MSGT_ERR_FMT_SYNTAX), (int)(ptr + 1 - fmt), fmt, ptr + 1);
				}
			}
			return -1;
		} else if (pFmt.state == PFMTS_STOP) {
			if (pFmt.type == 'v') {
				/* sensor value */
				if ((sizeof(TCHAR) * (size_t)(ptr - fmt)) >= sizeof(buf)) {
					if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_LENGTH), (int)(ptr + 1 - fmt), fmt, ptr + 1);
					return -1;
				}
				memcpy(buf, start, sizeof(TCHAR) * (size_t)(ptr - start - 1));
				buf[(size_t)(ptr - start - 1)] = 'f';
				buf[(size_t)(ptr - start)] = 0;
				switch (pFmt.subType) {
				case 'C': written = _ftprintf(fd, buf, temp); break;
				case 'F': written = _ftprintf(fd, buf, (temp * 1.8f) + 32.0f); break;
				case 'H': written = _ftprintf(fd, buf, rh); break;
				default:
					if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_SYNTAX), (int)(ptr + 1 - fmt), fmt, ptr + 1);
					return -1;
				}
			} else if (pFmt.type == '%' && last == '%') {
				/* escaped % */
				written = _ftprintf(fd, _T("%c"), '%');
			} else if (last == '%' || last == '#') {
				/* time value */
				memcpy(tFmt, start, sizeof(TCHAR) * (size_t)(ptr + 1 - start));
				tFmt[ptr + 1 - start] = 0;
				written = (int)_tcsftime(buf, sizeof(buf) / sizeof(*buf), tFmt, timeInfo);
				if (written <= 0) {
					/* error reported by API */
					if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_SYNTAX), (int)(ptr + 1 - fmt), fmt, ptr + 1);
					return -1;
				}
				written = _ftprintf(fd, _T("%.*s"), written, buf);
			} else {
				/* invalid format code syntax */
				if (verbose > 0) _ftprintf(ferr, MSGT(MSGT_ERR_FMT_SYNTAX), (int)(ptr + 1 - fmt), fmt, ptr + 1);
				return -1;
			}
			if (written < 0) return -1;
			res += written;
			start = ptr + 1;
			memset(&pFmt, 0, sizeof(pFmt));
		}
		last = c;
	}
	if (start != ptr) {
		/* print string literal (i.e. string part without any format codes) */
		written = _ftprintf(fd, _T("%.*s"), (int)(ptr - start), start);
		if (written < 0) return -1;
		res += written;
	}
	return res;
}


/**
 * The function lets the current thread sleep for the number
 * of milliseconds passed to it.
 *
 * @param[in] duration - time duration to sleep
 */
static void delay(const unsigned long duration) {
	if (duration == 0) return;
#if defined(PCF_IS_WIN)
	Sleep((DWORD)duration);
#elif defined(PCF_IS_LINUX)
	usleep((useconds_t)duration * 1000);
#endif
}
