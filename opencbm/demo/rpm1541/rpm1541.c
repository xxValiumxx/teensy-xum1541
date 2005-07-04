/*
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *  Copyright 1999-2005 Michael Klein <michael.klein@puffin.lb.shuttle.de>
 *  Copyright 2004-2005 Andreas Boose, Wolfgang Moser
 *  Copyright      2005 Spiro Trikaliotis
*/

#ifdef SAVE_RCSID
static char *rcsid =
    "@(#) $Id: rpm1541.c,v 1.1 2005-04-20 14:09:58 strik Exp $";
#endif


#include <opencbm.h>
#include <arch.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define RUNS 2

#define OCCSZ 10

static CBM_FILE fd = CBM_FILE_INVALID;

static void ARCH_SIGNALDECL
reset(int dummy)
{
    if (fd != CBM_FILE_INVALID)
    {
        cbm_reset(fd);
        cbm_driver_close(fd);
    }
    exit(1);
}

int ARCH_MAINDECL
main(int argc, char **argv)
{
    __u_char drive = 8;
    int i;
    unsigned char cmd[48];
    static unsigned char rpm_prog[] =
    {
#include "rpm1541.inc"
    };

    unsigned long  int count;
    unsigned short int currpm, tsize;
    struct
    {
      unsigned short int rpm;
      unsigned short int occurence;
    } occtable[OCCSZ];
    const char *type_str;


    if (cbm_driver_open(&fd, 0) != 0)
        return -1;

    if (argc > 1)
    {
      drive = arch_atoc(argv[1]);

      if (drive < 8 || drive > 11)
      {
          printf("Usage: %s [driveNo]\n\ndriveNo  - Commodore IEC bus"
                 " disk drive number (range: 8..11)\n", argv[0]);
          exit(0);
      }
    }
    if (cbm_identify(fd, drive, NULL, &type_str) == 0)
    {
        printf( "Using drive %2d, drive type string: %s\n", drive, type_str );
    }

    signal(SIGINT, reset);

    cbm_exec_command(fd, drive, "I0:", 0);
    cbm_upload(fd, drive, 0x0500, rpm_prog, sizeof(rpm_prog));

    sprintf(cmd, "M-R%c%c%c", 0x87, 0x00, 2);

    count = 1ul;
    tsize = 0;
    for (i = 0; i < OCCSZ; ++i)
    {
        occtable[i].rpm=occtable[i].occurence=0;
    }

    while (1)
    {
        unsigned char int_count[2];
        double rpm;

        cbm_exec_command(fd, drive, "U4:", 0);
        cbm_exec_command(fd, drive, cmd, 6);
        cbm_talk(fd, drive, 15);
        cbm_raw_read(fd, &int_count, 2);
        cbm_untalk(fd);

        rpm = (600000.0*RUNS) / (int_count[0] + 256 * int_count[1]);

/*      printf("%3.2lf\n", rpm); */
        currpm = (unsigned short int)(rpm*100 + .5);  /* rounded! */

        /* search occurency table for that value */
        for (i=0;i<OCCSZ;++i)
        {
            if (occtable[i].rpm == 0)
            {
                /* value not found, but an empty field */
                break;
            }
            else if (currpm == occtable[i].rpm)
            {
                /* value found, increase counter */
                occtable[i].occurence++;
                break;
            }
        }

        if (i >= OCCSZ || occtable[i].rpm==0)
        {
            if (i >= OCCSZ)
            {
                printf("\rNo space left in occurrency table for rpm value %3.2f\n", rpm);
            }
            else /* if (occtable[i].rpm==0) */
            {
                occtable[i].rpm=currpm;
                occtable[i].occurence++;
            }

            /* reprint table header either if the overflow warning
             * was printed or a new rpm value has been added
             */
            printf("\r%10s: %6s", "count", "curRPM");
            for (i = 0; i < OCCSZ && occtable[i].rpm != 0; ++i)
            {
                printf("|%3.2f", occtable[i].rpm / 100.0);
            }
            printf("|\n------------------");
            for (i=0;i<OCCSZ && occtable[i].rpm!=0;++i)
            {
                printf("+------");
            }
            printf("+\n");
        }

        printf("\r%10lu: %3.2f", count++, rpm);
        for (i=0;i<OCCSZ && occtable[i].rpm!=0;++i)
        {
            printf("|%6u", occtable[i].occurence);
        }
        printf("|");
        fflush(stdout);
        /* printf("%10lu: %3.2f %5u\n", count++, rpm, currpm); */
    }

    return 0;
}