/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/debug.h>
#include <winpr/environment.h>
#include <winpr/wlog.h>

#if defined(ANDROID)
#include <android/log.h>
#include "../log.h"
#endif

#include "wlog.h"

struct _wLogFilter
{
	DWORD Level;
	LPSTR* Names;
	DWORD NameCount;
};
typedef struct _wLogFilter wLogFilter;

#define WLOG_FILTER_NOT_FILTERED -1
#define WLOG_FILTER_NOT_INITIALIZED -2
/**
 * References for general logging concepts:
 *
 * Short introduction to log4j:
 * http://logging.apache.org/log4j/1.2/manual.html
 *
 * logging - Logging facility for Python:
 * http://docs.python.org/2/library/logging.html
 */

LPCSTR WLOG_LEVELS[7] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF" };

static INIT_ONCE _WLogInitialized = INIT_ONCE_STATIC_INIT;
static DWORD g_FilterCount = 0;
static wLogFilter* g_Filters = NULL;
static wLog* g_RootLog = NULL;

static wLog* WLog_New(LPCSTR name, wLog* rootLogger);
static void WLog_Free(wLog* log);
static LONG WLog_GetFilterLogLevel(wLog* log);
static int WLog_ParseLogLevel(LPCSTR level);
static BOOL WLog_ParseFilter(wLogFilter* filter, LPCSTR name);
static BOOL WLog_ParseFilters(void);

#if !defined(_WIN32)
static void WLog_Uninit_(void) __attribute__((destructor));
#endif

static void WLog_Uninit_(void)
{
	DWORD index;
	wLog* child = NULL;
	wLog* root = g_RootLog;

	if (!root)
		return;

	for (index = 0; index < root->ChildrenCount; index++)
	{
		child = root->Children[index];
		WLog_Free(child);
	}

	WLog_Free(root);
	g_RootLog = NULL;
}

static BOOL CALLBACK WLog_InitializeRoot(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context)
{
	char* env;
	DWORD nSize;
	DWORD logAppenderType;
	LPCSTR appender = "WLOG_APPENDER";

	if (!(g_RootLog = WLog_New("", NULL)))
		return FALSE;

	g_RootLog->IsRoot = TRUE;
	WLog_ParseFilters();
	logAppenderType = WLOG_APPENDER_CONSOLE;
	nSize = GetEnvironmentVariableA(appender, NULL, 0);

	if (nSize)
	{
		env = (LPSTR)malloc(nSize);

		if (!env)
			goto fail;

		if (GetEnvironmentVariableA(appender, env, nSize) != nSize - 1)
		{
			fprintf(stderr, "%s environment variable modified in my back", appender);
			free(env);
			goto fail;
		}

		if (_stricmp(env, "CONSOLE") == 0)
			logAppenderType = WLOG_APPENDER_CONSOLE;
		else if (_stricmp(env, "FILE") == 0)
			logAppenderType = WLOG_APPENDER_FILE;
		else if (_stricmp(env, "BINARY") == 0)
			logAppenderType = WLOG_APPENDER_BINARY;

#ifdef HAVE_SYSLOG_H
		else if (_stricmp(env, "SYSLOG") == 0)
			logAppenderType = WLOG_APPENDER_SYSLOG;

#endif /* HAVE_SYSLOG_H */
#ifdef HAVE_JOURNALD_H
		else if (_stricmp(env, "JOURNALD") == 0)
			logAppenderType = WLOG_APPENDER_JOURNALD;

#endif
		else if (_stricmp(env, "UDP") == 0)
			logAppenderType = WLOG_APPENDER_UDP;

		free(env);
	}

	if (!WLog_SetLogAppenderType(g_RootLog, logAppenderType))
		goto fail;

#if defined(_WIN32)
	atexit(WLog_Uninit_);
#endif
	return TRUE;
fail:
	free(g_RootLog);
	g_RootLog = NULL;
	return FALSE;
}

static BOOL log_recursion(LPCSTR file, LPCSTR fkt, int line)
{
	BOOL status = FALSE;
	char** msg = NULL;
	size_t used, i;
	void* bt = winpr_backtrace(20);
#if defined(ANDROID)
	LPCSTR tag = WINPR_TAG("utils.wlog");
#endif

	if (!bt)
		return FALSE;

	msg = winpr_backtrace_symbols(bt, &used);

	if (!msg)
		goto out;

#if defined(ANDROID)

	if (__android_log_print(ANDROID_LOG_FATAL, tag, "Recursion detected!!!") < 0)
		goto out;

	if (__android_log_print(ANDROID_LOG_FATAL, tag, "Check %s [%s:%d]", fkt, file, line) < 0)
		goto out;

	for (i = 0; i < used; i++)
		if (__android_log_print(ANDROID_LOG_FATAL, tag, "%zd: %s", i, msg[i]) < 0)
			goto out;

#else

	if (fprintf(stderr, "[%s]: Recursion detected!\n", fkt) < 0)
		goto out;

	if (fprintf(stderr, "[%s]: Check %s:%d\n", fkt, file, line) < 0)
		goto out;

	for (i = 0; i < used; i++)
		if (fprintf(stderr, "%s: %" PRIuz ": %s\n", fkt, i, msg[i]) < 0)
			goto out;

#endif
	status = TRUE;
out:
	free(msg);
	winpr_backtrace_free(bt);
	return status;
}

static BOOL WLog_Write(wLog* log, wLogMessage* message)
{
	BOOL status;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->active)
		if (!WLog_OpenAppender(log))
			return FALSE;

	if (!appender->WriteMessage)
		return FALSE;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		status = log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

static BOOL WLog_WriteData(wLog* log, wLogMessage* message)
{
	BOOL status;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->active)
		if (!WLog_OpenAppender(log))
			return FALSE;

	if (!appender->WriteDataMessage)
		return FALSE;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		status = log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteDataMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

static BOOL WLog_WriteImage(wLog* log, wLogMessage* message)
{
	BOOL status;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->active)
		if (!WLog_OpenAppender(log))
			return FALSE;

	if (!appender->WriteImageMessage)
		return FALSE;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		status = log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteImageMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

static BOOL WLog_WritePacket(wLog* log, wLogMessage* message)
{
	BOOL status;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->active)
		if (!WLog_OpenAppender(log))
			return FALSE;

	if (!appender->WritePacketMessage)
		return FALSE;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		status = log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WritePacketMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

BOOL WLog_PrintMessageVA(wLog* log, DWORD type, DWORD level, DWORD line, const char* file,
                         const char* function, va_list args)
{
	BOOL status = FALSE;
	wLogMessage message = { 0 };
	message.Level = level;
	message.LineNumber = line;
	message.FileName = file;
	message.FunctionName = function;

	switch (type)
	{
		case WLOG_MESSAGE_TEXT:
			message.FormatString = va_arg(args, const char*);

			if (!strchr(message.FormatString, '%'))
			{
				message.TextString = (LPSTR)message.FormatString;
				status = WLog_Write(log, &message);
			}
			else
			{
				char formattedLogMessage[WLOG_MAX_STRING_SIZE];

				if (wvsnprintfx(formattedLogMessage, WLOG_MAX_STRING_SIZE - 1, message.FormatString,
				                args) < 0)
					return FALSE;

				message.TextString = formattedLogMessage;
				status = WLog_Write(log, &message);
			}

			break;

		case WLOG_MESSAGE_DATA:
			message.Data = va_arg(args, void*);
			message.Length = va_arg(args, int);
			status = WLog_WriteData(log, &message);
			break;

		case WLOG_MESSAGE_IMAGE:
			message.ImageData = va_arg(args, void*);
			message.ImageWidth = va_arg(args, int);
			message.ImageHeight = va_arg(args, int);
			message.ImageBpp = va_arg(args, int);
			status = WLog_WriteImage(log, &message);
			break;

		case WLOG_MESSAGE_PACKET:
			message.PacketData = va_arg(args, void*);
			message.PacketLength = va_arg(args, int);
			message.PacketFlags = va_arg(args, int);
			status = WLog_WritePacket(log, &message);
			break;

		default:
			break;
	}

	return status;
}

BOOL WLog_PrintMessage(wLog* log, DWORD type, DWORD level, DWORD line, const char* file,
                       const char* function, ...)
{
	BOOL status;
	va_list args;
	va_start(args, function);
	status = WLog_PrintMessageVA(log, type, level, line, file, function, args);
	va_end(args);
	return status;
}

DWORD WLog_GetLogLevel(wLog* log)
{
	if (!log)
		return WLOG_OFF;

	if (log->FilterLevel <= WLOG_FILTER_NOT_INITIALIZED)
		log->FilterLevel = WLog_GetFilterLogLevel(log);

	if (log->FilterLevel > WLOG_FILTER_NOT_FILTERED)
		return (DWORD)log->FilterLevel;
	else if (log->Level == WLOG_LEVEL_INHERIT)
		log->Level = WLog_GetLogLevel(log->Parent);

	return log->Level;
}

BOOL WLog_IsLevelActive(wLog* _log, DWORD _log_level)
{
	DWORD level;

	if (!_log)
		return FALSE;

	level = WLog_GetLogLevel(_log);

	if (level == WLOG_OFF)
		return FALSE;

	return _log_level >= level;
}

BOOL WLog_SetStringLogLevel(wLog* log, LPCSTR level)
{
	int lvl;

	if (!log || !level)
		return FALSE;

	lvl = WLog_ParseLogLevel(level);

	if (lvl < 0)
		return FALSE;

	return WLog_SetLogLevel(log, (DWORD)lvl);
}

static BOOL WLog_reset_log_filters(wLog* log)
{
	DWORD x;

	if (!log)
		return FALSE;

	log->FilterLevel = WLOG_FILTER_NOT_INITIALIZED;

	for (x = 0; x < log->ChildrenCount; x++)
	{
		wLog* child = log->Children[x];

		if (!WLog_reset_log_filters(child))
			return FALSE;
	}

	return TRUE;
}

BOOL WLog_AddStringLogFilters(LPCSTR filter)
{
	DWORD pos;
	DWORD size;
	DWORD count;
	LPSTR p;
	LPSTR filterStr;
	LPSTR cp;
	wLogFilter* tmp;

	if (!filter)
		return FALSE;

	count = 1;
	p = (LPSTR)filter;

	while ((p = strchr(p, ',')) != NULL)
	{
		count++;
		p++;
	}

	pos = g_FilterCount;
	size = g_FilterCount + count;
	tmp = (wLogFilter*)realloc(g_Filters, size * sizeof(wLogFilter));

	if (!tmp)
		return FALSE;

	g_Filters = tmp;
	cp = (LPSTR)_strdup(filter);

	if (!cp)
		return FALSE;

	p = cp;
	filterStr = cp;

	do
	{
		p = strchr(p, ',');

		if (p)
			*p = '\0';

		if (pos < size)
		{
			if (!WLog_ParseFilter(&g_Filters[pos++], filterStr))
			{
				free(cp);
				return FALSE;
			}
		}
		else
			break;

		if (p)
		{
			filterStr = p + 1;
			p++;
		}
	} while (p != NULL);

	g_FilterCount = size;
	free(cp);
	return WLog_reset_log_filters(WLog_GetRoot());
}

static BOOL WLog_UpdateInheritLevel(wLog* log, DWORD logLevel)
{
	if (!log)
		return FALSE;

	if (log->inherit)
	{
		DWORD x;
		log->Level = logLevel;

		for (x = 0; x < log->ChildrenCount; x++)
		{
			wLog* child = log->Children[x];

			if (!WLog_UpdateInheritLevel(child, logLevel))
				return FALSE;
		}
	}

	return TRUE;
}

BOOL WLog_SetLogLevel(wLog* log, DWORD logLevel)
{
	DWORD x;

	if (!log)
		return FALSE;

	if ((logLevel > WLOG_OFF) && (logLevel != WLOG_LEVEL_INHERIT))
		logLevel = WLOG_OFF;

	log->Level = logLevel;
	log->inherit = (logLevel == WLOG_LEVEL_INHERIT) ? TRUE : FALSE;

	for (x = 0; x < log->ChildrenCount; x++)
	{
		wLog* child = log->Children[x];

		if (!WLog_UpdateInheritLevel(child, logLevel))
			return FALSE;
	}

	return WLog_reset_log_filters(log);
}

int WLog_ParseLogLevel(LPCSTR level)
{
	int iLevel = -1;

	if (!level)
		return -1;

	if (_stricmp(level, "TRACE") == 0)
		iLevel = WLOG_TRACE;
	else if (_stricmp(level, "DEBUG") == 0)
		iLevel = WLOG_DEBUG;
	else if (_stricmp(level, "INFO") == 0)
		iLevel = WLOG_INFO;
	else if (_stricmp(level, "WARN") == 0)
		iLevel = WLOG_WARN;
	else if (_stricmp(level, "ERROR") == 0)
		iLevel = WLOG_ERROR;
	else if (_stricmp(level, "FATAL") == 0)
		iLevel = WLOG_FATAL;
	else if (_stricmp(level, "OFF") == 0)
		iLevel = WLOG_OFF;

	return iLevel;
}

BOOL WLog_ParseFilter(wLogFilter* filter, LPCSTR name)
{
	char* p;
	char* q;
	int count;
	LPSTR names;
	int iLevel;
	count = 1;

	if (!name)
		return FALSE;

	p = (char*)name;

	if (p)
	{
		while ((p = strchr(p, '.')) != NULL)
		{
			count++;
			p++;
		}
	}

	names = _strdup(name);

	if (!names)
		return FALSE;

	filter->NameCount = count;
	filter->Names = (LPSTR*)calloc((count + 1UL), sizeof(LPSTR));

	if (!filter->Names)
	{
		free(names);
		filter->NameCount = 0;
		return FALSE;
	}

	filter->Names[count] = NULL;
	count = 0;
	p = (char*)names;
	filter->Names[count++] = p;
	q = strrchr(p, ':');

	if (!q)
	{
		free(names);
		free(filter->Names);
		filter->Names = NULL;
		filter->NameCount = 0;
		return FALSE;
	}

	*q = '\0';
	q++;
	iLevel = WLog_ParseLogLevel(q);

	if (iLevel < 0)
	{
		free(names);
		free(filter->Names);
		filter->Names = NULL;
		filter->NameCount = 0;
		return FALSE;
	}

	filter->Level = (DWORD)iLevel;

	while ((p = strchr(p, '.')) != NULL)
	{
		if (count < (int)filter->NameCount)
			filter->Names[count++] = p + 1;

		*p = '\0';
		p++;
	}

	return TRUE;
}

BOOL WLog_ParseFilters(void)
{
	LPCSTR filter = "WLOG_FILTER";
	BOOL res = FALSE;
	char* env;
	DWORD nSize;
	free(g_Filters);
	g_Filters = NULL;
	g_FilterCount = 0;
	nSize = GetEnvironmentVariableA(filter, NULL, 0);

	if (nSize < 1)
		return TRUE;

	env = (LPSTR)malloc(nSize);

	if (!env)
		return FALSE;

	if (GetEnvironmentVariableA(filter, env, nSize) == nSize - 1)
		res = WLog_AddStringLogFilters(env);

	free(env);
	return res;
}

LONG WLog_GetFilterLogLevel(wLog* log)
{
	DWORD i, j;
	BOOL match = FALSE;

	if (log->FilterLevel >= 0)
		return log->FilterLevel;

	for (i = 0; i < g_FilterCount; i++)
	{
		for (j = 0; j < g_Filters[i].NameCount; j++)
		{
			if (j >= log->NameCount)
				break;

			if (_stricmp(g_Filters[i].Names[j], "*") == 0)
			{
				match = TRUE;
				break;
			}

			if (_stricmp(g_Filters[i].Names[j], log->Names[j]) != 0)
				break;

			if (j == (log->NameCount - 1))
			{
				match = TRUE;
				break;
			}
		}

		if (match)
			break;
	}

	if (match)
		log->FilterLevel = g_Filters[i].Level;
	else
		log->FilterLevel = WLOG_FILTER_NOT_FILTERED;

	return log->FilterLevel;
}

static BOOL WLog_ParseName(wLog* log, LPCSTR name)
{
	char* p;
	int count;
	LPSTR names;
	count = 1;
	p = (char*)name;

	while ((p = strchr(p, '.')) != NULL)
	{
		count++;
		p++;
	}

	names = _strdup(name);

	if (!names)
		return FALSE;

	log->NameCount = count;
	log->Names = (LPSTR*)calloc((count + 1UL), sizeof(LPSTR));

	if (!log->Names)
	{
		free(names);
		return FALSE;
	}

	log->Names[count] = NULL;
	count = 0;
	p = (char*)names;
	log->Names[count++] = p;

	while ((p = strchr(p, '.')) != NULL)
	{
		if (count < (int)log->NameCount)
			log->Names[count++] = p + 1;

		*p = '\0';
		p++;
	}

	return TRUE;
}

wLog* WLog_New(LPCSTR name, wLog* rootLogger)
{
	wLog* log = NULL;
	char* env = NULL;
	DWORD nSize;
	int iLevel;
	log = (wLog*)calloc(1, sizeof(wLog));

	if (!log)
		return NULL;

	log->Name = _strdup(name);

	if (!log->Name)
		goto out_fail;

	if (!WLog_ParseName(log, name))
		goto out_fail;

	log->Parent = rootLogger;
	log->ChildrenCount = 0;
	log->ChildrenSize = 16;
	log->FilterLevel = WLOG_FILTER_NOT_INITIALIZED;

	if (!(log->Children = (wLog**)calloc(log->ChildrenSize, sizeof(wLog*))))
		goto out_fail;

	log->Appender = NULL;

	if (rootLogger)
	{
		log->Level = WLOG_LEVEL_INHERIT;
		log->inherit = TRUE;
	}
	else
	{
		LPCSTR level = "WLOG_LEVEL";
		log->Level = WLOG_INFO;
		nSize = GetEnvironmentVariableA(level, NULL, 0);

		if (nSize)
		{
			env = (LPSTR)malloc(nSize);

			if (!env)
				goto out_fail;

			if (GetEnvironmentVariableA(level, env, nSize) != nSize - 1)
			{
				fprintf(stderr, "%s environment variable changed in my back !\n", level);
				free(env);
				goto out_fail;
			}

			iLevel = WLog_ParseLogLevel(env);
			free(env);

			if (iLevel >= 0)
			{
				if (!WLog_SetLogLevel(log, (DWORD)iLevel))
					goto out_fail;
			}
		}
	}

	iLevel = WLog_GetFilterLogLevel(log);

	if (iLevel >= 0)
	{
		if (!WLog_SetLogLevel(log, (DWORD)iLevel))
			goto out_fail;
	}

	return log;
out_fail:
	free(log->Children);
	free(log->Name);
	free(log);
	return NULL;
}

void WLog_Free(wLog* log)
{
	if (log)
	{
		if (log->Appender)
		{
			WLog_Appender_Free(log, log->Appender);
			log->Appender = NULL;
		}

		free(log->Name);
		free(log->Names[0]);
		free(log->Names);
		free(log->Children);
		free(log);
	}
}

wLog* WLog_GetRoot(void)
{
	if (!InitOnceExecuteOnce(&_WLogInitialized, WLog_InitializeRoot, NULL, NULL))
		return NULL;

	return g_RootLog;
}

static BOOL WLog_AddChild(wLog* parent, wLog* child)
{
	if (parent->ChildrenCount >= parent->ChildrenSize)
	{
		wLog** tmp;
		parent->ChildrenSize *= 2;

		if (!parent->ChildrenSize)
		{
			if (parent->Children)
				free(parent->Children);

			parent->Children = NULL;
		}
		else
		{
			tmp = (wLog**)realloc(parent->Children, sizeof(wLog*) * parent->ChildrenSize);

			if (!tmp)
			{
				if (parent->Children)
					free(parent->Children);

				parent->Children = NULL;
				return FALSE;
			}

			parent->Children = tmp;
		}
	}

	if (!parent->Children)
		return FALSE;

	parent->Children[parent->ChildrenCount++] = child;
	child->Parent = parent;
	return TRUE;
}

static wLog* WLog_FindChild(LPCSTR name)
{
	DWORD index;
	wLog* root;
	wLog* child = NULL;
	BOOL found = FALSE;
	root = WLog_GetRoot();

	if (!root)
		return NULL;

	for (index = 0; index < root->ChildrenCount; index++)
	{
		child = root->Children[index];

		if (strcmp(child->Name, name) == 0)
		{
			found = TRUE;
			break;
		}
	}

	return (found) ? child : NULL;
}

wLog* WLog_Get(LPCSTR name)
{
	wLog* log;

	if (!(log = WLog_FindChild(name)))
	{
		wLog* root = WLog_GetRoot();

		if (!root)
			return NULL;

		if (!(log = WLog_New(name, root)))
			return NULL;

		if (!WLog_AddChild(root, log))
		{
			WLog_Free(log);
			return NULL;
		}
	}

	return log;
}

BOOL WLog_Init(void)
{
	return WLog_GetRoot() != NULL;
}

BOOL WLog_Uninit(void)
{
	return TRUE;
}
