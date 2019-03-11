/* $Id: bootsec.c,v 1.4 2004/07/24 00:49:05 pasha Exp $ */

static char *SCCSID = "@(#)1.7  8/27/98 14:34:41 src/jfs/utils/libfs/bootsec.c, jfslib, w45.fs32, 990417.1";
/* 
 *   Copyright (c) International Business Machines  Corp., 2000
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software 
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/* History
 *
 * PS16032004 - insert boot code
 * PS21032004 - insert IBM fixes
 */
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DOSDEVIOCTL
#include <os2.h>
#include <jfs_types.h>
#include <jfs_aixisms.h>
#include <stdio.h>
#include <string.h>
#include <sysbloks.h>
#include <extboot.h>
#include "devices.h"
#include "debug.h"
#include "bootcode.h"

#ifdef DEBUG_BOOTSEC
void print_BPB(
struct BPB *    bpb)
{
        printf("\tbytes_per_sector = %d\n", bpb->bytes_per_sector);
        printf("\tsectors_per_cluser = %d\n", bpb->sectors_per_cluster);
        printf("\treserved_sectors = %d\n", bpb->reserved_sectors);
        printf("\tnbr_fats = %d\n", bpb->nbr_fats);
        printf("\troot_entries = %d\n", bpb->root_entries);
        printf("\ttotal_sectors = %d\n", bpb->total_sectors);
        printf("\tmedia_type = %d\n", bpb->media_type);
        printf("\tsectors_per_fat = %d\n", bpb->sectors_per_fat);
        printf("\tsectors_per_track = %d\n", bpb->sectors_per_track);
        printf("\tnumber_of_heads = %d\n", bpb->number_of_heads);
        printf("\thidden_sectors = %d\n", bpb->hidden_sectors);
        printf("\tlarge_total_sectors = %d\n", bpb->large_total_sectors);
}
#endif


uint32 checksum(uint8 *string, uint32 len)
{
        /* Since we're going to be dealing with 11 bytes, let's rotate 3 bits
         *  at a time to ensure that we use a whole uint32
         */

        uint32 sum = 0;

        while (len--) {
                /* rotate sum three bits left and add char*/
                sum = (sum << 3) + (sum >> 29);
                sum += *string++;
        }
        return sum;
}
extern char *Vol_Label;
int32 write_bootsec(
HFILE           fd,
struct DPB *    dpb,
char *          vol_label,
int32           islog)
{
        ULONG                   actual, bytesPsec;
        struct Extended_Boot *  ext_boot;
        unsigned short       *  bootsec_buf=(unsigned short *)BootCode;   //PS16032004
        int32                   i;
        int32                   rc;
        DATETIME                time;
        struct BPB          *   bpb=&dpb->dev_bpb;
//PS21032004 Begin
        uint32                  VolSerNum;     // Volume Serial Number

        if(islog == 2)                         // called from chkdsk
          {
           ext_boot = (struct Extended_Boot *)bootsec_buf;
           bytesPsec = bpb->bytes_per_sector;
           rc = ujfs_rw_diskblocks(fd, 0, bytesPsec, (void *)bootsec_buf, GET);
           if (rc != NO_ERROR)
              {
               printf("write_bootsec: error reading Extended Boot Structure.\n");
               return 1;
               }
           VolSerNum = *(uint32*)(&ext_boot->Boot_Serial);
           }                               
//PS21032004 End

#ifdef DEBUG_BOOTSEC
        rc = DosSetFilePtr(fd, 0, FILE_BEGIN, &actual) ||
             DosRead(fd, &ext_boot, sizeof(ext_boot), &actual);

        if (rc != NO_ERROR) {
                printf("write_bootsec: error reading Extended Boot Structure.\n");
                return 1;
        }

        printf("\nExtended Boot Structure");
        if (islog == 1 )                            //PS21032004
                printf(" of log device");

        printf(":\nBoot_jmp = 0x%2x 0x%2x 0x%2x\n", ext_boot.Boot_jmp[0],
               ext_boot.Boot_jmp[1], ext_boot.Boot_jmp[2]);
        printf("Boot_OEM =");
        for (i = 0; i < 8; i++)
                printf(" 0x%2x", ext_boot.Boot_OEM[i]);
        printf("\nBoot_BPB:\n");
        print_BPB((struct BPB *)&(ext_boot.Boot_BPB));
        printf("Boot_DriveNumber = %d\n", ext_boot.Boot_DriveNumber);
        printf("Boot_CurrentHead = %d\n", ext_boot.Boot_CurrentHead);
        printf("Boot_Sig = %d\n", ext_boot.Boot_Sig);
        printf("Boot_Serial =");
        for (i = 0; i < 4; i++)
                printf(" 0x%2x", ext_boot.Boot_Serial[i]);
        printf("\nBoot_Vol_Label = %s\n", ext_boot.Boot_Vol_Label);
        printf("Boot_System_ID = %s\n\n", ext_boot.Boot_System_ID);
        printf("BPB returned by DosDevIOCtl:\n");
        print_BPB(bpb);
#endif /* DEBUG_BOOTSEC */

        bytesPsec = bpb->bytes_per_sector;
        ext_boot = (struct Extended_Boot *)bootsec_buf;
	 memcpy(ext_boot->Boot_OEM, "IBM 4.50", 8);
        memcpy(&ext_boot->Boot_BPB, bpb, sizeof(struct Extended_BPB));
        ext_boot->Boot_DriveNumber = 0x80;          //PS16032004
        if(Vol_Label)
          {
          ext_boot->Boot_CurrentHead = 0x80 + (*Vol_Label & 0x5F) - 'C';   // Calculate Drive Letter for miniFSD
          }
        else
          ext_boot->Boot_CurrentHead = 0x80;
        ext_boot->Boot_Sig = 41;                        /* That's what the manual says  */
        /*
         * Serial number will be a checksum of the date & time
         */
//PS21032004 Begin
        if(islog != 2)
          {
           DosGetDateTime(&time);
           *(uint32*)(&ext_boot->Boot_Serial) = checksum((uint8 *)&time, sizeof(time));
           }
        else                                  // called from chkdsk
           *(uint32*)(&ext_boot->Boot_Serial) = VolSerNum;
//PS21032004 End
        
        strncpy(ext_boot->Boot_Vol_Label, vol_label, 11);
        if (islog == 1 )                       //PS21032004
                memcpy(ext_boot->Boot_System_ID, "JFSLOG  ",8);
        else
                memcpy(ext_boot->Boot_System_ID, "JFS     ",8);

	/* Some kind of signature that needs to be at the end of the sector */
//PS16032004	bootsec_buf[255] = 0xaa55;

        if ( !islog )                       //PS16032004
           {
            bytesPsec=((TOTALCODE >> 9) << 9);   
            if(bytesPsec < TOTALCODE) bytesPsec += 512;
           }
        rc = ujfs_rw_diskblocks(fd, 0, bytesPsec, (void *)bootsec_buf, PUT);

        if (rc)
                DBG_ERROR(("Internal Error: error writing boot sector!\n"))
// printf("Write %d bytes rc=%d Vol:%s - %s\n",bytesPsec,rc,vol_label,Vol_Label);
        return rc;
}
