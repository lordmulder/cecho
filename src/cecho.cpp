///////////////////////////////////////////////////////////////////////////////
// cecho - colored echo for Windows command processo
// Copyright (C) 2014 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version, but always including the *additional*
// restrictions defined in the "License.txt" file.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#include <Windows.h>

#define COLOR_ERROR (BACKGROUND_RED | BACKGROUND_INTENSITY)
#define COLOR_WHBGR (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY)

#define MALLOC(SIZE) HeapAlloc(g_processHeap, 0, (SIZE))
#define FREE(PTR)    HeapFree(g_processHeap, 0, (PTR))

static HANDLE g_consoleOutput = NULL;
static HANDLE g_processHeap = NULL;
static bool g_isTerminalAttached = false;
static UINT g_previousOutputCP = CP_ACP;
static WORD g_defaultTextAttribute = 0;

// -----------------------------------------------------------------------------
// Color Names
// -----------------------------------------------------------------------------

static const struct
{
	wchar_t num;
	wchar_t *name;
	WORD fg_code;
	WORD bg_code;
}
colors_lut[] =
{
	{
		L'0', L"black",
		0x0000, 0x0000
	},
	{
		L'1', L"white",
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
		BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY
	},
	{
		L'2', L"grey",
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
		BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE
	},
	{
		L'3', L"red",
		FOREGROUND_RED | FOREGROUND_INTENSITY,
		BACKGROUND_RED | BACKGROUND_INTENSITY
	},
	{
		L'4', L"dark_red",
		FOREGROUND_RED,
		BACKGROUND_RED
	},
	{
		L'5', L"green",
		FOREGROUND_GREEN | FOREGROUND_INTENSITY,
		BACKGROUND_GREEN | BACKGROUND_INTENSITY
	},
	{
		L'6', L"dark_green",
		FOREGROUND_GREEN,
		BACKGROUND_GREEN
	},
	{
		L'7', L"blue",
		FOREGROUND_BLUE | FOREGROUND_INTENSITY,
		BACKGROUND_BLUE | BACKGROUND_INTENSITY
	},
	{
		L'8', L"dark_blue",
		FOREGROUND_BLUE,
		BACKGROUND_BLUE
	},
	{
		L'9', L"yellow",
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
		BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY
	},
	{
		L'a', L"dark_yellow",
		FOREGROUND_RED | FOREGROUND_GREEN,
		BACKGROUND_RED | BACKGROUND_GREEN
	},
	{
		L'b', L"magenta",
		FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY,
		BACKGROUND_BLUE | BACKGROUND_RED | BACKGROUND_INTENSITY
	},
	{
		L'c', L"dark_magenta",
		FOREGROUND_BLUE | FOREGROUND_RED,
		BACKGROUND_BLUE | BACKGROUND_RED
	},
	{
		L'd', L"cyan",
		FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
		BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY
	},
	{
		L'e', L"dark_cyan",
		FOREGROUND_BLUE | FOREGROUND_GREEN,
		BACKGROUND_BLUE | BACKGROUND_GREEN
	},
	{ 0, NULL, 0, 0, }
};

// -----------------------------------------------------------------------------
// Escape Chars
// -----------------------------------------------------------------------------

static const struct
{
	char escape;
	char replace;
	char *info;
}
escape_lut[] =
{
	{ 'n',  '\n', "Line feed" },
	{ 't',  '\t', "Tabulator" },
	{ '\\', '\\', "Backslash" },
	{ 0x0,  0x0, NULL}
};

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

static char *utf16_to_utf8(const wchar_t* str)
{
	const int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);

	if(len > 0)
	{
		char *buffer = (char*) MALLOC(len);
		const int result = WideCharToMultiByte(CP_UTF8, 0, str, -1, buffer, len, NULL, NULL);

		if((result > 0) && (result <= len))
		{
			return buffer;
		}

		FREE(buffer);
	}

	return NULL;
}

static bool parse_color_helper(WORD &color, const wchar_t *text, const bool fg)
{
	color &= (fg ? 0xFFF0 : 0xFF0F);

	for(size_t i = 0; colors_lut[i].name; i++)
	{
		if(lstrcmpiW(text, colors_lut[i].name) == 0)
		{
			color |= (fg ? colors_lut[i].fg_code : colors_lut[i].bg_code);
			return true;
		}
	}

	wchar_t colorNum = text[0];
	CharLowerW(reinterpret_cast<LPWSTR>(colorNum));

	for(size_t i = 0; colors_lut[i].name; i++)
	{
		if(colors_lut[i].num == colorNum)
		{
			color |= (fg ? colors_lut[i].fg_code : colors_lut[i].bg_code);
			return true;
		}
	}

	return false;
}

static WORD parse_color(const wchar_t *fg_color, const wchar_t *bg_color)
{
	WORD color = g_defaultTextAttribute & 0xFF;

	if(fg_color)
	{
		if(!parse_color_helper(color, fg_color, true))
		{
			return MAXWORD;
		}
	}

	if(bg_color)
	{
		if(!parse_color_helper(color, bg_color, false))
		{
			return MAXWORD;
		}
	}

	return color;
}

void escape_chars(char *str)
{
	char *pos_rd = str;
	char *pos_wr = str;

	while(*pos_rd)
	{
		if(*pos_rd == '\\')
		{
			if(*(++pos_rd))
			{
				for(size_t k = 0; escape_lut[k].escape; k++)
				{
					if(*pos_rd == escape_lut[k].escape)
					{
						*(pos_wr++) = escape_lut[k].replace;
					}
				}
				pos_rd++;
			}
			continue;
		}
		if(pos_rd != pos_wr)
		{
			*pos_wr = *pos_rd;
		}
		pos_rd++;
		pos_wr++;
	}

	*pos_wr = '\0';
}

// -----------------------------------------------------------------------------
// Print Function
// -----------------------------------------------------------------------------

static void print_colored(const WORD color, const char *text, const bool lineFeed = true)
{
	static const char EOL = '\n';
	const DWORD len = lstrlenA(text);
	DWORD charsWritten;
	
	if(g_isTerminalAttached)
	{
		SetConsoleTextAttribute(g_consoleOutput, (g_defaultTextAttribute & 0xFF00) | color);
		WriteConsoleA(g_consoleOutput, text, len, &charsWritten, NULL);
		if(lineFeed)
		{
			WriteConsoleA(g_consoleOutput, &EOL, 1, &charsWritten, NULL);
		}
		SetConsoleTextAttribute(g_consoleOutput, g_defaultTextAttribute);
	}
	else
	{
		WriteFile(g_consoleOutput, text, len, &charsWritten, NULL);
		if(lineFeed)
		{
			WriteFile(g_consoleOutput, &EOL, 1, &charsWritten, NULL);
		}
		FlushFileBuffers(g_consoleOutput);
	}
}

static void print_formated(const WORD color, const char *text, ...)
{
	char buffer[MAX_PATH];
	va_list args;
	va_start(args, text);
	wvsprintfA(buffer, text, args);
	va_end(args);
	print_colored(color, buffer, false);
}

// -----------------------------------------------------------------------------
// Command Help
// -----------------------------------------------------------------------------

static void print_help(void)
{
	print_colored(g_defaultTextAttribute, "cecho - colored echo for Windows command processor [" __DATE__ "]");
	print_colored(g_defaultTextAttribute, "Copyright (C) 2014 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.\n");
	print_colored(g_defaultTextAttribute, "Usage:");
	print_colored(g_defaultTextAttribute, "   cecho.exe <forground_color> [<background_color>] <echo_text>\n");
	print_colored(g_defaultTextAttribute, "Color names:");
	for(size_t i = 0; colors_lut[i].name; i++)
	{
		if(char *temp = utf16_to_utf8(colors_lut[i].name))
		{
			print_formated(g_defaultTextAttribute, "   %C --> %12S --> ", colors_lut[i].num, colors_lut[i].name);
			print_colored(colors_lut[i].fg_code, "########", false);
			print_colored(colors_lut[i].fg_code | COLOR_WHBGR, "########");
			FREE(temp);
		}
	}
	print_colored(g_defaultTextAttribute, "\nEscape chars:");
	for(size_t i = 0; escape_lut[i].info; i++)
	{
		print_formated(g_defaultTextAttribute, "   \\%C --> %s\n", escape_lut[i].escape, escape_lut[i].info);
	}
	print_colored(g_defaultTextAttribute, "\nExample:");
	print_colored(g_defaultTextAttribute, "   cecho.exe blue yellow \"Hello world!\\nGoodbye cruel world!\"");
}

// -----------------------------------------------------------------------------
// Main Function
// -----------------------------------------------------------------------------

static int cecho_main(int argc, wchar_t* argv[])
{
	if((argc < 2) || (lstrcmpiW(argv[1], L"--help") == 0) || (lstrcmpiW(argv[1], L"-h") == 0) || (lstrcmpiW(argv[1], L"/?") == 0))
	{
		print_help();
		return EXIT_SUCCESS;
	}

	if(argc < 3 || argc > 4 )
	{
		print_colored(COLOR_ERROR, "cecho error: invalid arguments specified!");
		return EXIT_FAILURE;
	}

	const WORD color = (argc > 3) ? parse_color(argv[1], argv[2]) : parse_color(argv[1], NULL);

	if(color == MAXWORD)
	{
		print_colored(COLOR_ERROR, "cecho error: unknown color name specified!");
		return EXIT_FAILURE;
	}

	if(char *temp = utf16_to_utf8((argc > 3) ? argv[3] : argv[2]))
	{
		escape_chars(temp);
		print_colored(color, temp);
		FREE(temp);
	}

	return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------
// Application Entry Point
// -----------------------------------------------------------------------------

int wmain(int argc, wchar_t* argv[])
{
	g_consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	
	if((g_consoleOutput == NULL) || (g_consoleOutput == INVALID_HANDLE_VALUE))
	{
		FatalAppExitA(0, "System error: Standard output handle not available!");
	}
	
	g_isTerminalAttached = (GetFileType(g_consoleOutput) == FILE_TYPE_CHAR);
	
	if(g_isTerminalAttached)
	{
		g_previousOutputCP = GetConsoleOutputCP();
		SetConsoleOutputCP(CP_UTF8);

		CONSOLE_SCREEN_BUFFER_INFOEX screenBufferInfo;
		SecureZeroMemory(&screenBufferInfo, sizeof(CONSOLE_SCREEN_BUFFER_INFOEX));
		screenBufferInfo.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);

		if(GetConsoleScreenBufferInfoEx(g_consoleOutput, &screenBufferInfo))
		{
			g_defaultTextAttribute = screenBufferInfo.wAttributes;
		}
	}

	const int exit_code = cecho_main(argc, argv);

	if(g_isTerminalAttached)
	{
		SetConsoleOutputCP(g_previousOutputCP);
		SetConsoleTextAttribute(g_consoleOutput, g_defaultTextAttribute);
	}
	
	return exit_code;
}

static LONG WINAPI unhandled_exception_handler(struct _EXCEPTION_POINTERS*)
{
	FatalAppExitA(0, "Unhandeled exception error detected!");
	return LONG_MAX;
}

int wmainCRTStartup(void)
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
	SetUnhandledExceptionFilter(unhandled_exception_handler);

	g_processHeap = GetProcessHeap();
	if((g_processHeap == NULL) || (g_processHeap == INVALID_HANDLE_VALUE))
	{
		FatalAppExitA(0, "System error: Process heap not available!");
	}

	int numArgs;
	LPWSTR *szArglist = CommandLineToArgvW(GetCommandLineW(), &numArgs);

	if(szArglist == NULL)
	{
		FatalAppExitA(0, "System error: Command line parser has failed!");
	}

	const int exit_code = wmain(numArgs, szArglist);
	LocalFree(szArglist);

	return exit_code;
}
