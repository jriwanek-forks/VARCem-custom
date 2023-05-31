/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement the external ROM loader.
 *		This loader defines a 'ROM set' to be one or more images
 *		of ROM chip(s), where all properties of these defined in
 *		a single 'ROM definition' (text) file.
 *
 * NOTE:	This file uses a fairly generic script parser, which can
 *		be re-used for others parts. This would mean passing the
 *		'parser' function a pointer to either a command handler,
 *		or to use a generic handler, and then pass it a pointer
 *		to a command table. For now, we don't.
 *
 * Version:	@(#)rom_load.c	1.0.17	2021/03/18
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2018-2021 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <ctype.h>
#include <86box/86box.h>
#include <86box/machine.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/plat.h>


#define MAX_ARGS	16		/* max number of arguments */


/* Process a single (logical) command line. */
static int
process(int ln, int argc, char **argv, romdef_t *r, const wchar_t *path)
{
    wchar_t temp[1024];

again:
    if (! strcmp(argv[0], "size")) {
	/* Total size of image. */
	r->total = get_val(argv[1]);
    } else if (! strcmp(argv[0], "offset")) {
	/* Offset into the ROM area. */
	r->offset = get_val(argv[1]);
    } else if (! strcmp(argv[0], "mode")) {
	/* Loading method to use for this image. */
	if (! strcmp(argv[1], "linear"))
		r->mode = 0;
	  else if (! strcmp(argv[1], "interleaved"))
		r->mode = 1;
	  else {
		ERRLOG("ROM: invalid mode '%s' on line %i.\n", argv[1], ln);
		return(0);
	}
    } else if (! strcmp(argv[0], "optional")) {
	/*
	 * This is an optional file.
	 * Next word is the name of the configuration
	 * variable this depends on, for example "basic"
	 * or "romdos".
	 */
	if (! machine_get_config_int(argv[1])) return(1);

	/* Skip the keyword and variable name, and re-parse. */
	argv += 2;
	argc -= 2;
	goto again;
    } else if (! strcmp(argv[0], "file")) {
	/* Specify the image filename and/or additional parameters. */
	mbstowcs(temp, argv[1], sizeof_w(temp));
	plat_append_filename(r->files[r->nfiles].path, path, temp);
	r->files[r->nfiles].skip = 0;
	r->files[r->nfiles].offset = r->offset;
	r->files[r->nfiles].size = r->total;
	switch(argc) {
		case 5:
			r->files[r->nfiles].size = get_val(argv[4]);
			/*FALLTHROUGH*/

		case 4:
			r->files[r->nfiles].offset = get_val(argv[3]);
			/*FALLTHROUGH*/

		case 3:
			r->files[r->nfiles].skip = get_val(argv[2]);
			break;
	}
	r->nfiles++;
    } else if (! strcmp(argv[0], "font")) {
	/* Load a video controller font. */
	r->fontnum = atoi(argv[1]);
	mbstowcs(r->fontfn, argv[2], sizeof_w(r->fontfn));
    } else if (! strcmp(argv[0], "md5sum")) {
	/* Skip for now. */
    } else if (! strcmp(argv[0], "video")) {
	/* Load a video controller BIOS. */
	mbstowcs(r->vidfn, argv[1], sizeof_w(r->vidfn));
	sscanf(argv[2], "%i", &r->vidsz);
    } else {
	ERRLOG("ROM: invalid command '%s' on line %i.\n", argv[0], ln);
	return(0);
    }

    return(1);
}


/* Parse a script file, and call the command handler for each command. */
static int
parser(FILE *fp, romdef_t *r, const wchar_t *path)
{
    char line[1024];
    char *args[MAX_ARGS];
    int doskip, doquot;
    int skipnl, dolit;
    int a, c, l;
    char *sp;

    /* Initialize the parser and run. */
    l = 0;
    for (;;) {
	/* Clear the per-line stuff. */
	skipnl = dolit = doquot = 0;
	doskip = 1;
	for (a = 0; a < MAX_ARGS; a++)
		args[a] = NULL;
	a = 0;
	sp = line;

	/* Catch the first argument. */
	args[a] = sp;

	/*
	 * Get the next logical line from the configuration file.
	 * This is not trivial, since we allow for a fairly standard
	 * Unix-style ASCII-based text file, including the usual line
	 * extenders and character escapes.
	 */
	while ((c = fgetc(fp)) != EOF) {
		/* Literals come first... */
		if (dolit) {
			switch(c) {
				case '\r':	/* raw CR, ignore it */
					continue;

				case '\n':	/* line continuation! */
					l++;
					break;

				case 'n':
					*sp++ = '\n';
					break;

				case 'r':
					*sp++ = '\r';
					break;

				case 'b':
					*sp++ = '\b';
					break;

				case 'e':
					*sp++ = 27;
					break;

				case '"':
					*sp++ = '"';
					break;

				case '#':
					*sp++ = '#';
					break;

				case '!':
					*sp++ = '!';
					break;

				case '\\':
					*sp++ = '\\';
					break;

				default:
					ERRLOG("ROM: syntax error: escape '\\%c'", c);
					*sp++ = '\\';
					*sp++ = (char)c;
			}
			dolit = 0;
			continue;

		}

		/* Are we in a comment? */
		if (skipnl) {
			if (c == '}')
				skipnl--;	/* nested comment closed */

			if (c == '\n')
				skipnl = 0;	/* end of line! */

			if (skipnl == 0)
				break;

			continue;		/* skip more... */
		}

		/* Are we starting a comment? */
		if ((c == '{') || (c == ';') || (c == '#')) {
			skipnl++;
			continue;
		}

		/* Are we starting a literal character? */
		if (c == '\\') {
			dolit = 1;
			continue;
		}

		/* Are we starting a quote? */
		if (c == '"') {
			doquot = (1 - doquot);
			if (doskip) {
				args[a++] = sp;
				doskip = 0;
			}
			continue;
		}

		/* Quoting means raw insertion. */
		if (doquot) {
			/* We are quoting, so insert as is. */
			if (c == '\n')
				ERRLOG("ROM: syntax error: unexpected newline, expected (\")\n");
			*sp++ = (char)c;
			continue;
		}

		/*
		 * Everything else, normal character insertion.
		 * This means, we process each character as if it
		 * was typed in.  We assume the usual rules for
		 * skipping whitespace, blank lines and so forth.
		 */
		/* Ignore raw CRs, they are common in DOS-originated files. */
		if (c == '\r')
			continue;

		/* A raw newline terminates the logical line. */
		if (c == '\n') break;

		/* See if this is some word breaker. */
		if (c == ',') {
			/* Terminate current word. */
			*sp++ = '\0';

			/* ... and start new word. */
			args[a] = sp;
			doskip = 0;
			continue;
		}

		/* Is it regular whitespace? */
		if ((c == ' ') || (c == '\t')) {
			/* Are we skipping whitespace? */
			if (! doskip) {
				/* Nope, Start a new argument. */
				*sp++ = '\0';
				doskip = 1;
			}

			/* Yes, skip it. */
			continue;
		}

		/* Just a regular thingie. Store it. */
		if (isupper(c))
			c = tolower(c);

		/* If we are skipping space, we now hit the end of that. */
		if (doskip)
			args[a++] = sp;
		*sp++ = (char)c;
		doskip = 0;
	}
	*sp = '\0';
	if (feof(fp)) break;
	if (ferror(fp)) {
		ERRLOG("ROM: Read Error on line %i\n", l);
		return(0);
	}
	l++;

	/* Ignore comment lines and empty lines. */
	if (*args[0] == '\0') continue;

	/* Process this line. */
	if (! process(l, a, args, r, path)) return(0);
    }

    return(1);
}


/*
 * If a ROM file could not be found, try to find its path
 * in the 'relocation' database, in case it was moved to
 * a different path.
 */
static FILE *
check_moved(wchar_t *wanted)
{
    char buff[1024], temp[1024];
    wchar_t path[1024];
    char *cp, *p = NULL;
    int blen;
    FILE *fp;

    /* Open the database. */
    swprintf(path, sizeof_w(path), L"%ls/%ls", MOVED_PATH, MOVED_FILE);
    if ((fp = rom_fopen(path, L"r")) == NULL) {
	/* Redirection also failed, give up. */
	ERRLOG("ROM: relocation database '%ls not found, giving up!\n", path);
	return(NULL);
    }

    /* Get a copy of the wanted filename, and trim it. */
    wcstombs(temp, wanted, sizeof(temp));
    if ((cp = strrchr(temp, '/')) != NULL)
	*cp++ = '\0';

    /* Now read through it, ignoring comments and blank lines. */
    memset(buff, 0x00, sizeof(buff));
    for (;;) {
	/* Read one line and clean it. */
	if (fgets(buff, sizeof(buff), fp) == NULL)
		break;
	if ((p = strchr(buff, '\r')) != NULL)
		*p = '\0';
	if ((p = strchr(buff, '\n')) != NULL)
		*p = '\0';
	if (buff[0] == '#' || buff[0] == '\0')
		continue;

	/* Get the first part of the line. */
	p = buff;
	while (*p && *p != ' ' && *p != '\t')
		p++;
	*p++ = '\0';
	blen = strlen(buff);

	/* Now skip more whitespace, up to the second part of the line. */
	while (*p == ' ' || *p == '\t')
		p++;

	/* If this does not match what we need, try next line. */
	if (strncmp(buff, temp, blen) == 0) {
		/* Close the database file. */
		(void)fclose(fp);

		/* Create new pathname. */
		swprintf(path, sizeof_w(path), L"%s/%ls", p, &wanted[blen+1]);
		if ((fp = rom_fopen(path, L"r")) == NULL) {
			ERRLOG("ROM: error opening relocated script '%ls'\n",
									path);
			return(NULL);
		}

		/* All good, update caller's path! */
		wcscpy(wanted, path);

		return(fp);
	}
    }

    /* Close the database file. */
    (void)fclose(fp);

    return(NULL);
}


/* Load a BIOS ROM image into memory. */
int
rom_load_bios(romdef_t *r, const wchar_t *fn, int test_only)
{
    wchar_t path[1024], temp[1024];
    wchar_t script[1024], *sp;
    FILE *fp;
    int c, i;

    /* Clear ROM definition and default to a 64K ROM BIOS image. */
    memset(r, 0x00, sizeof(romdef_t));
    r->total = 65536;
    r->fontnum = -1;

    /* Generate the BIOS pathname. */
    wcscpy(path, fn);

    /* Generate the full script pathname. */
    plat_append_filename(script, path, BIOS_FILE);
    pc_path(script, sizeof_w(script), NULL);

    if (! test_only)
	INFO("ROM: loading script '%ls'\n", script);

    /* Open the script file. */
    if ((fp = rom_fopen(script, L"r")) == NULL) {
	/* Check for a redirection, in case it was moved. */
	if ((fp = check_moved(script)) == NULL) {
		/* Redirection also failed, give up. */
		ERRLOG("ROM: unable to open '%ls'\n", script);
		return(0);
	} else {
		if (! test_only)
			INFO("ROM: loading relocated script '%ls'\n", script);

	}
    }

    /* Re-generate the BIOS pathname. */
    if ((sp = wcsrchr(script, L'/')) != NULL)
	*sp = L'\0';
    wcscpy(path, script);

    /* Parse and process the file. */
    i = parser(fp, r, path);
    (void)fclose(fp);
    if (i == 0) {
	ERRLOG("ROM: error in script '%ls'\n", script);
	return(0);
    }

    /* All done if just testing for presence. */
    if (test_only)
	return(1);

    /* Show the resulting data. */
    INFO(" Size     : %lu\n", r->total);
    INFO(" Offset   : 0x%06lx (%lu)\n", r->offset, r->offset);
    INFO(" Mode     : %s\n", (r->mode == 1)?"interleaved":"linear");
    INFO(" Files    : %i\n", r->nfiles);
    for (c = 0; c < r->nfiles; c++) {
	INFO(" %c[%i]     : '%ls', %i, 0x%06lx, %i\n",
		(r->files[c].offset==0xffffffff)?'*':' ', c+1,
		r->files[c].path, r->files[c].skip,
		r->files[c].offset, r->files[c].size);
    }
    if (r->fontnum != -1)
	INFO(" Font     : %i, '%ls'\n", r->fontnum, r->fontfn);
    if (r->vidsz != 0)
	INFO(" VideoBIOS: '%ls', %i\n", r->vidfn, r->vidsz);

    /* Actually perform the work. */
    switch(r->mode) {
	case 0:			/* linear file(s) */
		/* We loop on all files. */
		for (c = 0; c < r->nfiles; c++) {
			/* If this is a no-load file, skip. */
			if (r->files[c].offset == 0xffffffff)
				continue;

			pc_path(script, sizeof_w(script), r->files[c].path);

			i = rom_load_linear(script,
					    r->files[c].offset,
					    r->files[c].size,
					    r->files[c].skip, bios);
			if (i != 1) break;
		}
		break;

	case 1:			/* interleaved file(s) */
		/* We loop on all files. */
		for (c = 0; c < r->nfiles; c += 2) {
			/* If this is a no-load file, skip. */
			if (r->files[c].offset == 0xffffffff)
				continue;

			pc_path(script, sizeof_w(script), r->files[c].path);
			pc_path(temp, sizeof_w(temp), r->files[c+1].path);

			i = rom_load_interleaved(script, temp,
					 	 r->files[c].offset,
					 	 r->files[c].size,
					 	 r->files[c].skip, bios);
			if (i != 1) break;
		}
		break;
    }

    /* Update BIOS mask. */
    if (r->total >= 0x010000)
	biosmask = (r->total - 1);
    else
	biosmask = 0x00ffff;

    /* Create a full pathname for the video font file. */
    if (r->fontnum != -1) {
	plat_append_filename(temp, path, r->fontfn);
	pc_path(r->fontfn, sizeof_w(r->fontfn), temp);
    }

    /* Create a full pathname for the video BIOS file. */
    if (r->vidsz != 0) {
	plat_append_filename(temp, path, r->vidfn);
	pc_path(r->vidfn, sizeof_w(r->vidfn), temp);
    }

    INFO("ROM: total %lu, mask 0x%06lx\n", r->total, biosmask);

    return(1);
}
