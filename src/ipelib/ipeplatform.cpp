// --------------------------------------------------------------------
// Platform dependent methods
// --------------------------------------------------------------------
/*

    This file is part of the extensible drawing editor Ipe.
    Copyright (C) 1993-2016  Otfried Cheong

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, you have permission to link Ipe with the
    CGAL library and distribute executables, as long as you follow the
    requirements of the Gnu General Public License in regard to all of
    the software in the executable aside from CGAL.

    Ipe is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
    License for more details.

    You should have received a copy of the GNU General Public License
    along with Ipe; if not, you can find it at
    "http://www.gnu.org/copyleft/gpl.html", or write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "ipebase.h"
#include "ipeattributes.h"

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#include <gdiplus.h>
#else
#include <sys/wait.h>
#include <xlocale.h>
#include <dirent.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdarg>
#include <unistd.h>
#include <clocale>

using namespace ipe;

// --------------------------------------------------------------------

/*! \class ipe::Platform
  \ingroup base
  \brief Platform dependent methods.
*/

//! Return the Ipelib version.
/*! This is available as a function so that one can verify what
  version of Ipelib one has actually linked with (as opposed to the
  header files used during compilation).
*/
int Platform::libVersion()
{
  return IPELIB_VERSION;
}

// --------------------------------------------------------------------

static bool initialized = false;
static bool showDebug = false;
static Platform::DebugHandler debugHandler = nullptr;

#ifdef WIN32
static ULONG_PTR gdiplusToken = 0;
// not yet available in MINGW RT library
// _locale_t ipeLocale;
#else
locale_t ipeLocale;
#endif

#ifdef WIN32
static void readIpeConf()
{
  String fname = Platform::ipeDir("", nullptr);
  fname += "\\ipe.conf";
  String conf = Platform::readFile(fname);
  if (conf.empty())
    return;
  ipeDebug("ipe.conf = %s", conf.z());
  int i = 0;
  while (i < conf.size()) {
    String line = conf.getLine(i);
    std::vector<wchar_t> wline;
    Platform::utf8ToWide(line.z(), wline);
    _wputenv(&wline[0]);
  }
}
#endif

static void debugHandlerImpl(const char *msg)
{
  if (showDebug) {
    fprintf(stderr, "%s\n", msg);
#ifdef WIN32
    fflush(stderr);
    OutputDebugStringA(msg);
#endif
  }
}

static void shutdownIpelib()
{
#ifdef WIN32
  Gdiplus::GdiplusShutdown(gdiplusToken);
  // _free_locale(ipeLocale);
#else
  freelocale(ipeLocale);
#endif
  Repository::cleanup();
}

//! Initialize Ipelib.
/*! This method must be called before Ipelib is used.

  It creates a LC_NUMERIC locale set to 'C', which is necessary for
  correct loading and saving of Ipe objects.  The method also checks
  that the correct version of Ipelib is loaded, and aborts with an
  error message if the version is not correct.  Also enables ipeDebug
  messages if environment variable IPEDEBUG is defined.  (You can
  override this using setDebug).
*/
void Platform::initLib(int version)
{
  if (initialized)
    return;
  initialized = true;
  showDebug = false;
  if (getenv("IPEDEBUG") != 0) {
    showDebug = true;
    fprintf(stderr, "Debug messages enabled\n");
  }
  debugHandler = debugHandlerImpl;
#ifdef WIN32
  readIpeConf();
  // ipeLocale = ::_create_locale(LC_NUMERIC, "C");
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, 0);
#else
  ipeLocale = newlocale(LC_NUMERIC_MASK, "C", NULL);
#endif
  atexit(shutdownIpelib);
#ifndef WIN32
  if (version == IPELIB_VERSION)
    return;
  fprintf(stderr,
	  "Ipetoipe has been compiled with header files for Ipelib %d\n"
	  "but is dynamically linked against libipe %d.\n"
	  "Check with 'ldd' which libipe is being loaded, and "
	  "replace it by the correct version or set LD_LIBRARY_PATH.\n",
	  version, IPELIB_VERSION);
  exit(99);
#endif
}

//! Enable or disable display of ipeDebug messages.
void Platform::setDebug(bool debug)
{
  showDebug = debug;
}

// --------------------------------------------------------------------

void ipeDebug(const char *msg, ...) noexcept
{
  if (debugHandler) {
    char buf[8196];
    va_list ap;
    va_start(ap, msg);
    std::vsprintf(buf, msg, ap);
    va_end(ap);
    debugHandler(buf);
  }
}

// --------------------------------------------------------------------

//! Returns current working directory.
/*! Returns empty string if something fails. */
String Platform::currentDirectory()
{
#ifdef WIN32
  wchar_t *buffer = _wgetcwd(0, 0);
  return String(buffer);
#else
  char buffer[1024];
  if (getcwd(buffer, 1024) != buffer)
    return String();
  return String(buffer);
#endif
}

#ifdef IPEBUNDLE
String Platform::ipeDir(const char *suffix, const char *fname)
{
#ifdef WIN32
  wchar_t exename[OFS_MAXPATHNAME];
  GetModuleFileNameW(0, exename, OFS_MAXPATHNAME);
  String exe(exename);
#else
  char path[PATH_MAX];
  uint32_t size = sizeof(path);
  String exe;
  if (_NSGetExecutablePath(path, &size) == 0) {
    char *rp = realpath(path, NULL);
    exe = String(rp);
    free(rp);
  } else
    ipeDebug("ipeDir: buffer too small; need size %u", size);
#endif
  int i = exe.rfind(IPESEP);
  if (i >= 0) {
    exe = exe.left(i);    // strip filename
    i = exe.rfind(IPESEP);
    if (i >= 0) {
      exe = exe.left(i);  // strip bin directory name
    }
  }
#ifdef __APPLE__
  if (!strcmp(suffix, "doc"))
    exe += "/SharedSupport/";
  else
    exe += "/Resources/";
#else
  exe += IPESEP;
#endif
  exe += suffix;
  if (fname) {
    exe += IPESEP;
    exe += fname;
  }
  return exe;
}
#endif

#ifndef WIN32
static String dotIpe()
{
  const char *home = getenv("HOME");
  if (!home)
    return String();
  String res = String(home) + "/.ipe";
  if (!Platform::fileExists(res) && mkdir(res.z(), 0700) != 0)
    return String();
  return res + "/";
}
#endif

//! Return path for the directory containing pdflatex and xelatex.
/*! If empty means look on PATH. */
String Platform::latexPath()
{
  String result;
#ifdef WIN32
  const wchar_t *p = _wgetenv(L"IPELATEXPATH");
  if (p)
    result = String(p);
#else
  char *p = getenv("IPELATEXPATH");
  if (p)
    result = p;
#endif
  return result;
}

//! Returns directory for running Latex.
/*! The directory is created if it does not exist.  Returns an empty
  string if the directory cannot be found or cannot be created.
  The directory returned ends in the path separator.
 */
String Platform::latexDirectory()
{
#ifdef WIN32
  String latexDir;
  const wchar_t *p = _wgetenv(L"IPELATEXDIR");
  if (p)
    latexDir = String(p);
  else {
    wchar_t szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA,
				   NULL, 0, szPath))) {
      latexDir = String(szPath) + "\\ipe";
    } else {
      p = _wgetenv(L"LOCALAPPDATA");
      if (p)
	latexDir = String(p) + "\\ipe";
      else
	latexDir = ipeDir("latexrun");
    }
  }
  if (latexDir.right(1) == "\\")
    latexDir = latexDir.left(latexDir.size() - 1);
  if (!fileExists(latexDir)) {
    if (Platform::mkdir(latexDir.z()) != 0)
      return String();
  }
  latexDir += "\\";
  return latexDir;
#else
  const char *p = getenv("IPELATEXDIR");
  String latexDir;
  if (p) {
    latexDir = p;
    if (latexDir.right(1) == "/")
      latexDir = latexDir.left(latexDir.size() - 1);
  } else {
    latexDir = dotIpe() + "latexrun";
  }
  if (!fileExists(latexDir) && mkdir(latexDir.z(), 0700) != 0)
    return String();
  latexDir += "/";
  return latexDir;
#endif
}

//! Determine whether file exists.
bool Platform::fileExists(String fname)
{
#ifdef WIN32
  std::vector<wchar_t> wname;
  utf8ToWide(fname.z(), wname);
  return (_waccess(&wname[0], F_OK) == 0);
#else
  return (access(fname.z(), F_OK) == 0);
#endif
}

//! List all files in directory
/*! Return true if successful, false on error. */
bool Platform::listDirectory(String path, std::vector<String> &files)
{
#ifdef WIN32
  String pattern = path + "\\*";
  std::vector<wchar_t> wname;
  utf8ToWide(pattern.z(), wname);
  struct _wfinddata_t info;
  intptr_t h = _wfindfirst(&wname[0], &info);
  if (h == -1L)
    return false;
  files.push_back(String(info.name));
  while (_wfindnext(h, &info) == 0)
    files.push_back(String(info.name));
  _findclose(h);
  return true;
#else
  DIR *dir = opendir(path.z());
  if (dir == 0)
    return false;
  struct dirent *entry = readdir(dir);
  while (entry != 0) {
    String s(entry->d_name);
    if (s != "." && s != "..")
      files.push_back(s);
    entry = readdir(dir);
  }
  closedir(dir);
  return true;
#endif
}

//! Read entire file into string.
/*! Returns an empty string if file cannot be found or read.
  There is no way to distinguish an empty file from this. */
String Platform::readFile(String fname)
{
  std::FILE *file = Platform::fopen(fname.z(), "rb");
  if (!file)
    return String();
  String s;
  int ch;
  while ((ch = std::fgetc(file)) != EOF)
    s.append(ch);
  std::fclose(file);
  return s;
}

//! Runs latex on file text.tex in given directory.
int Platform::runLatex(String dir, LatexType engine) noexcept
{
  const char *latex = (engine == LatexType::Xetex) ?
    "xelatex" : (engine == LatexType::Luatex) ?
    "lualatex" : "pdflatex";
#ifdef WIN32
  if (getenv("IPETEXFORMAT")) {
    latex = (engine == LatexType::Xetex) ?
      "xetex ^&latex" : (engine == LatexType::Luatex) ?
      "luatex ^&latex" : "pdftex ^&pdflatex";
  }

  String bat;
  if (dir.size() > 2 && dir[1] == ':') {
    bat += dir.substr(0, 2);
    bat += "\r\n";
  }
  bat += "cd \"";
  bat += dir;
  bat += "\"\r\n";
  String path = latexPath();
  if (!path.empty()) {
    bat += "PATH ";
    bat += path;
    bat += ";%PATH%\r\n";
  }
  bat += latex;
  bat += " ipetemp.tex\r\n";
  // bat += "pause\r\n";  // alternative for Wine

  // CMD.EXE input needs to be encoded in "OEM codepage",
  // which can be different from "Windows codepage"
  std::vector<wchar_t> wbat;
  utf8ToWide(bat.z(), wbat);
  Buffer oemBat(2 * wbat.size() + 1);
  CharToOemW(&wbat[0], oemBat.data());

  String s = dir + "runlatex.bat";
  std::FILE *f = Platform::fopen(s.z(), "wb");
  if (!f)
    return -1;
  std::fputs(oemBat.data(), f);
  std::fclose(f);

  // Declare and initialize process blocks
  PROCESS_INFORMATION processInformation;
  STARTUPINFOW startupInfo;

  memset(&processInformation, 0, sizeof(processInformation));
  memset(&startupInfo, 0, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);

  // Call the executable program
  s = String("cmd /c call \"") + dir + String("runlatex.bat\"");
  utf8ToWide(s.z(), wbat);

  int result = CreateProcessW(NULL, &wbat[0], NULL, NULL, FALSE,
			      NORMAL_PRIORITY_CLASS|CREATE_NO_WINDOW,
			      NULL, NULL, &startupInfo, &processInformation);
  if (result == 0)
    return -1;  // failure to create process

  // Wait until child process exits.
  WaitForSingleObject(processInformation.hProcess, INFINITE);

  // Close process and thread handles.
  CloseHandle(processInformation.hProcess);
  CloseHandle(processInformation.hThread);

  // Apparently WaitForSingleObject doesn't work in Wine
  const char *wine = getenv("IPEWINE");
  if (wine)
    Sleep(Lex(wine).getInt());
  return 0;
#else
  if (getenv("IPETEXFORMAT")) {
    latex = (engine == LatexType::Xetex) ?
      "xetex \\&latex" : (engine == LatexType::Luatex) ?
      "luatex \\&latex" : "pdftex \\&pdflatex";
  }
  String s("cd \"");
  s += dir;
  s += "\"; rm -f ipetemp.log; ";
  String path = latexPath();
  if (path.empty())
    s += latex;
  else
    s += String("\"") + path + "/" + latex + "\"";
  s += " ipetemp.tex > /dev/null";
  int result = std::system(s.z());
  if (result != -1)
    result = WEXITSTATUS(result);
  return result;
#endif
}

#ifdef WIN32
FILE *Platform::fopen(const char *fname, const char *mode)
{
  std::vector<wchar_t> wname;
  utf8ToWide(fname, wname);
  std::vector<wchar_t> wmode;
  utf8ToWide(mode, wmode);
  return _wfopen(&wname[0], &wmode[0]);
}

int Platform::mkdir(const char *dname)
{
  std::vector<wchar_t> wname;
  utf8ToWide(dname, wname);
  return _wmkdir(&wname[0]);
}

String::String(const wchar_t *wbuf)
{
  if (!wbuf) {
    iImp = emptyString();
  } else {
    int rm = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
    iImp = new Imp;
    iImp->iRefCount = 1;
    iImp->iSize = rm - 1; // rm includes the trailing zero
    iImp->iCapacity = (rm + 32) & ~15;
    iImp->iData = new char[iImp->iCapacity];
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, iImp->iData, rm, NULL, NULL);
  }
}

void Platform::utf8ToWide(const char *utf8, std::vector<wchar_t> &wbuf)
{
  int rw = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  wbuf.resize(rw);
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &wbuf[0], rw);
}
#endif

// --------------------------------------------------------------------

double Platform::toDouble(String s)
{
#ifdef WIN32
  return strtod(s.z(), 0);
  // return _strtod_l(s.z(), 0, ipeLocale);
#else
  return strtod_l(s.z(), 0, ipeLocale);
#endif
}

void ipeAssertionFailed(const char *file, int line, const char *assertion)
{
  fprintf(stderr, "Assertion failed on line #%d (%s): '%s'\n",
	  line, file, assertion);
  abort();
  // exit(-1);
}

// --------------------------------------------------------------------
