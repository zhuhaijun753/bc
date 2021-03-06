/*
 * This file is part of nmealib.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <nmealib/context.h>
#include <nmealib/info.h>
#include <nmealib/nmath.h>
#include <nmealib/parser.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <libgen.h>
#include <unistd.h>
#endif

static const char * traceStr = "Trace: ";
static const char * errorStr = "Error: ";
static const char * eol = "\n";

static void trace(const char *str, size_t str_size) {
  write(1, traceStr, strlen(traceStr));
  write(1, str, str_size);
  write(1, eol, strlen(eol));
}
static void error(const char *str, size_t str_size) {
  write(1, errorStr, strlen(errorStr));
  write(1, str, str_size);
  write(1, eol, strlen(eol));
}

int main(int argc __UNUSED_PARAM, char *argv[] __UNUSED_PARAM) {
  char fn[2048];
  const char * filename = &fn[0];
  const char * deffile;
  NmeaInfo info;
  NmeaParser parser;
  FILE *file;
  char buff[2048];
  size_t it = 0;
  NmeaPosition dpos;
  int endoffile;

  if (argc <= 1) {
    deffile = "gpslog.txt";
	snprintf(&fn[0], sizeof(fn), "%s", deffile);
  } else {
    deffile = argv[1];
	snprintf(&fn[0], sizeof(fn), "%s", deffile);
  }
  printf("Using file %s\n", filename);

  file = fopen(filename, "rb");

  if (!file) {
    printf("Could not open file %s\n", filename);
    return -1;
  }

  nmeaContextSetTraceFunction(&trace);
  nmeaContextSetErrorFunction(&error);

  nmeaInfoClear(&info);
  nmeaParserInit(&parser, 0);

  fseek(file, 0, SEEK_SET);

  while (!(endoffile=feof(file))) {
  	memset(buff, 0, sizeof(buff));
    size_t size = fread(&buff[0], 1, 100, file);

	printf("sz = %d, s = %s\n", size, buff);
    nmeaParserParse(&parser, &buff[0], size, &info);
    nmeaMathInfoToPosition(&info, &dpos);

    printf("*** %03lu, Lat: %f, Lon: %f, Sig: %d, Fix: %d\n", (unsigned long) it++, dpos.lat, dpos.lon, info.sig,
        info.fix);
  }

  printf("endoffile = %d\n", endoffile);
  fseek(file, 0, SEEK_SET);

  /*
   }
   */

  fclose(file);

  nmeaParserDestroy(&parser);

  return 0;
}
