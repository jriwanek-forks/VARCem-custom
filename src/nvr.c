/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement a generic NVRAM/CMOS/RTC device.
 *
 * Version:	@(#)nvr.c	1.0.23	2020/10/10
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include <time.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/config.h>
#include <86box/timer.h>
#include <86box/machine.h>
#include <86box/plat.h>
#include <86box/nvr.h>


int	nvr_dosave;		/* NVR is dirty, needs saved */


static const int8_t days_in_month[12] = {
    31,28,31,30,31,30,31,31,30,31,30,31
};
static nvr_t	*saved_nvr = NULL;


/* Determine whether or not the year is leap. */
int
nvr_is_leap(int year)
{
    if (year % 400 == 0) return(1);
    if (year % 100 == 0) return(0);
    if (year % 4 == 0) return(1);

    return(0);
}


/* Determine the days in the current month. */
int
nvr_get_days(int month, int year)
{
    if (month != 2)
	return(days_in_month[month - 1]);

    return(nvr_is_leap(year) ? 29 : 28);
}


/* One more second has passed, update the internal clock. */
static void
rtc_tick(nvr_t *nvr)
{
    /* Ping the internal clock. */
    if (++nvr->clk.tm_sec == 60) {
	nvr->clk.tm_sec = 0;
	if (++nvr->clk.tm_min == 60) {
		nvr->clk.tm_min = 0;
    		if (++nvr->clk.tm_hour == 24) {
			nvr->clk.tm_hour = 0;
    			if (++nvr->clk.tm_mday == (nvr_get_days(nvr->clk.tm_mon,
							nvr->clk.tm_year) + 1)) {
				nvr->clk.tm_mday = 1;
    				if (++nvr->clk.tm_mon == 13) {
					nvr->clk.tm_mon = 1;
					nvr->clk.tm_year++;
				}
			}
		}
	}
    }
}


/* This is the RTC one-second timer. */
static void
onesec_timer(priv_t priv)
{
    nvr_t *nvr = (nvr_t *)priv;

    if (++nvr->onesec_cnt >= 100) {
	/* Update the internal clock. */
	rtc_tick(nvr);

	/* Update the RTC device if needed. */
	if (nvr->tick != NULL)
		(*nvr->tick)(nvr);

	nvr->onesec_cnt = 0;
    }

    nvr->onesec_time += (tmrval_t)(TIMER_USEC * 10000);
}


/* Initialize the generic NVRAM/RTC device. */
void
nvr_init(nvr_t *nvr)
{
    wchar_t tempw[256];
    char temp[64];
    struct tm *tm;
    wchar_t *sp;
    time_t now;
    FILE *fp;
    int c;

    /* Set up the NVR file's name unless we already have one. */
    if (nvr->fn == NULL) {
	/*
	 * First, let us try if a file exists with the name of
	 * the current configuration file in it. Such a 'named
	 * NVR' is sometimes needed in situations where multiple
	 * configurations share a single machine.
	 *
	 * Get the filename part of the configuration file path.
	 */
	sp = wcsrchr(cfg_path, L'/');
	if (sp != NULL)
		wcscpy(tempw, ++sp);
	else
		wcscpy(tempw, cfg_path);

	/* Drop the suffix. */
	sp = wcsrchr(tempw, L'.');
	if (sp != NULL)
		*sp = L'\0';

	/* Add our suffix. */
	wcscat(tempw, NVR_FILE_EXT);

	/* Try to open this file. */
	if ((fp = plat_fopen(nvr_path(tempw), L"rb")) != NULL) {
		/* It exists, use it. */
		(void)fclose(fp);
	} else {
		/* Does not exist, use the default name. */
		strcpy(temp, machine_get_internal_name());
		mbstowcs(tempw, temp, sizeof_w(tempw));
		wcscat(tempw, NVR_FILE_EXT);
	}
    } else {
	/* Already have a name, use that. */
	strcpy(temp, (const char *)nvr->fn);
	mbstowcs(tempw, temp, sizeof_w(tempw));
	wcscat(tempw, NVR_FILE_EXT);
    }

    /* Either way, we now have a usable filename. */
    c = (int)wcslen(tempw) + 1;
    sp = (wchar_t *)mem_alloc(c * sizeof(wchar_t));
    wcscpy(sp, tempw);
    nvr->fn = (const wchar_t *)sp;

    /* Initialize the internal clock as needed. */
    if (config.time_sync != TIME_SYNC_DISABLED) {
	/* Get the current time of day, and convert to local time. */
	(void)time(&now);

	if (config.time_sync == TIME_SYNC_ENABLED_UTC)
		tm = gmtime(&now);
	  else
		tm = localtime(&now);

	/* Set the internal clock. */
	nvr_time_set(tm, nvr);
    } else {
	/* Reset the internal clock to 1980/01/01 00:00. */
	nvr->clk.tm_mon = 1;
	nvr->clk.tm_year = 1980;
    }

    /* Set up our timer. */
    timer_add(onesec_timer, nvr, &nvr->onesec_time, TIMER_ALWAYS_ENABLED);

    /* It does not need saving yet. */
    nvr_dosave = 0;

    /* Save the NVR data pointer. */
    saved_nvr = nvr;

    /* Try to load the saved data. */
    (void)nvr_load();
}


/* Close an NVR, releasing its memory. */
void
nvr_close(nvr_t *nvr)
{
    wchar_t *str = (wchar_t *)nvr->fn;

    if (str != NULL)
        free(str);

    free(nvr);

    saved_nvr = NULL;
}


/* Get path to the NVR folder. */
wchar_t *
nvr_path(const wchar_t *str)
{
    static wchar_t temp[1024];

    /* Get the full prefix in place. */
    wcscpy(temp, usr_path);
    wcscat(temp, NVR_PATH);

    /* Create the directory if needed. */
    if (! plat_dir_check(temp))
	plat_dir_create(temp);

    /* Now append the actual filename. */
    plat_append_slash(temp);
    wcscat(temp, str);

    /* Make sure path is clean. */
    pc_path(temp, sizeof_w(temp), NULL);

    return(temp);
}


/*
 * Load an NVR from file.
 *
 * This function does two things, really. It clears and initializes
 * the RTC and NVRAM areas, sets up defaults for the RTC part, and
 * then attempts to load data from a saved file.
 *
 * Either way, after that, it will continue to configure the local
 * RTC to operate, so it can update either the local RTC, and/or
 * the one supplied by a client.
 */
int
nvr_load(void)
{
    wchar_t *path;
    FILE *fp;

    /* Make sure we have been initialized. */
    if (saved_nvr == NULL) return(0);

    /* Set the defaults. */
    if (saved_nvr->reset != NULL)
	saved_nvr->reset(saved_nvr);

    /* Load the (relevant) part of the NVR contents. */
    if (saved_nvr->size != 0) {
	path = nvr_path(saved_nvr->fn);
	INFO("NVR: loading from '%ls'\n", path);
	fp = plat_fopen(path, L"rb");
	if (fp != NULL) {
		/* Read NVR contents from file. */
		(void)fread(saved_nvr->regs, saved_nvr->size, 1, fp);
		(void)fclose(fp);
	}
    }

    /* Get the local RTC running! */
    if (saved_nvr->start != NULL)
	saved_nvr->start(saved_nvr);

    return(1);
}


/* Save the current NVR to a file. */
int
nvr_save(void)
{
    wchar_t *path;
    FILE *fp;

    /* Make sure we have been initialized. */
    if (config_ro || saved_nvr == NULL) return(0);

    if (saved_nvr->size != 0) {
	path = nvr_path(saved_nvr->fn);
	INFO("NVR: saving to '%ls'\n", path);
	fp = plat_fopen(path, L"wb");
	if (fp != NULL) {
		/* Save NVR contents to file. */
		(void)fwrite(saved_nvr->regs, saved_nvr->size, 1, fp);
		fclose(fp);
	}
    }

    /* Device is clean again. */
    nvr_dosave = 0;

    return(1);
}


/* Get current time from internal clock. */
void
nvr_time_get(const nvr_t *nvr, intclk_t *clk)
{
    uint8_t dom, mon, sum, wd;
    uint16_t cent, yr;

    clk->tm_sec = nvr->clk.tm_sec;
    clk->tm_min = nvr->clk.tm_min;
    clk->tm_hour = nvr->clk.tm_hour;
     dom = nvr->clk.tm_mday;
     mon = nvr->clk.tm_mon;
     yr = (nvr->clk.tm_year % 100);
     cent = ((nvr->clk.tm_year - yr) / 100) % 4;
     sum = dom+mon+yr+cent;
     wd = ((sum + 6) % 7);
    clk->tm_wday = wd;
    clk->tm_mday = nvr->clk.tm_mday;
    clk->tm_mon = (nvr->clk.tm_mon - 1);
    clk->tm_year = (nvr->clk.tm_year - 1900);
}


/* Set internal clock time. */
void
nvr_time_set(const intclk_t *clk, nvr_t *nvr)
{
    nvr->clk.tm_sec = clk->tm_sec;
    nvr->clk.tm_min = clk->tm_min;
    nvr->clk.tm_hour = clk->tm_hour;
    nvr->clk.tm_mday = clk->tm_mday;
    nvr->clk.tm_wday = clk->tm_wday;
    nvr->clk.tm_mon = (clk->tm_mon + 1);
    nvr->clk.tm_year = (clk->tm_year + 1900);
}
