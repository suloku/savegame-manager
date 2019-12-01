/*
    Most of the following code is based of:

    MirageIslandValue v1.5 - Tool to find the specific Mirage Island Value in
   RSE save files Copyright (C) 2015  BlackShark

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "languages.h"
#include "supported_games.h"

#define WC_OFFSET_E 0x56C;
#define WC_SCRIPT_OFFSET_E 0x8A8;
#define WC_OFFSET_FRLG 0x460;
#define WC_SCRIPT_OFFSET_FRLG 0x79C;

#define WC_OFFSET_E_JAP 0x490;
#define WC_SCRIPT_OFFSET_E_JAP 0x8A8;
#define WC_OFFSET_FRLG_JAP 0x384;
#define WC_SCRIPT_OFFSET_FRLG_JAP 0x79C;

#define ME3_OFFSET_E 0x8A8;
#define ME3_SIZE_E 1012;
#define ME3_SCRIPT_SIZE_E ME3_SIZE_E - 8;
#define ME3_OFFSET_RS 0x810;
#define ME3_SIZE_RS 1012;
#define ME3_SCRIPT_SIZE_RS 1004;
#define ME3_ITEM_SIZE 8;

#define ME3_SIZE 1012;

// check for the nocash signature
unsigned int checkNocash(char *sav) {
  unsigned int noCash;

  if (sav[0x0] == 0x4E && sav[0x01] == 0x6F && sav[0x02] == 0x63 &&
      sav[0x03] == 0x61 && sav[0x04] == 0x73 && sav[0x05] == 0x68) {
    noCash = 0x4C;
  } else {
    noCash = 0x0;
  }
  return noCash;
}

// find current save by comparing both save counters from first and second save
// block
int oldSav = 0;
unsigned int checkCurrentSav(char *sav) {
  unsigned long savIndex1, savIndex2;
  unsigned int currentSav, nocash;

  nocash = checkNocash(sav);

  savIndex1 = (sav[0xFFF + nocash] << 24 & 0xFFFFFFFF) +
              (sav[0xFFE + nocash] << 16 & 0xFFFFFF) +
              (sav[0xFFD + nocash] << 8 & 0xFFFF) +
              (sav[0xFFC + nocash] & 0xFF);
  savIndex2 = (sav[0xEFFF + nocash] << 24 & 0xFFFFFFFF) +
              (sav[0xEFFE + nocash] << 16 & 0xFFFFFF) +
              (sav[0xEFFD + nocash] << 8 & 0xFFFF) +
              (sav[0xEFFC + nocash] & 0xFF);
  if (savIndex1 >= savIndex2) {
    currentSav = 0x0 + nocash;
    oldSav = 0xE000 + nocash;
  } else if (savIndex1 < savIndex2) {
    currentSav = 0xE000 + nocash;
    oldSav = 0x0 + nocash;
  }

  return currentSav;
}
#define DATALEN 3968

// From Kaphoticc's PSavFixV2
int Chksum(int length, int *Data) {
  int Chk, i, tmp;
  length = length >> 2;
  Chk = 0;
  for (i = 0; i < length; i++) Chk += Data[i];

  tmp = Chk >> 16;
  tmp += Chk;

  Chk = (tmp & 0xFFFF);

  return Chk;
}

int wc_inject(char *sav, char *wc3, SupportedGames games, Language language)
{
  unsigned int currentSav = 0, sec[14] = {}, sec0, s0, sx, x;
  int wc_offset = 0x0;
  int wc_script_offset = 0x0;

  switch (language) {
    case JAPANESE:
      switch (games) {
        case EMERALD:
          wc_offset = WC_OFFSET_E_JAP;
          wc_script_offset = WC_SCRIPT_OFFSET_E_JAP;
          break;
        case FIRE_RED_AND_LEAF_GREEN:
          wc_offset = WC_OFFSET_FRLG_JAP;
          wc_script_offset = WC_SCRIPT_OFFSET_FRLG_JAP;
          break;
      }
      break;
    default:
      switch (games) {
        case EMERALD:
          wc_offset = WC_OFFSET_E;
          wc_script_offset = WC_SCRIPT_OFFSET_E;
          break;
        case FIRE_RED_AND_LEAF_GREEN:
          wc_offset = WC_OFFSET_FRLG;
          wc_script_offset = WC_SCRIPT_OFFSET_FRLG;
          break;
      }
      break;
  }

  // check where the current save is
  currentSav = checkCurrentSav(sav);

  // get section locations in current save
  for (s0 = 0; s0 <= 13; s0++) {
    sec0 = sav[0x0FF4 + 0x1000 * s0 + currentSav];
    if (sec0 == 0x0) {
      for (sx = 0; sx <= 13; sx++) {
        if ((s0 + sx) <= 13) {
          sec[sx] = s0 + sx;
        } else {
          sec[sx] = s0 + sx - 14;
        }
      }
      break;
    }
  }

  // check if all sections were found
  for (x = 0; x <= 13; x++) {
    if (sav[0x0FF4 + 0x1000 * sec[x] + currentSav] != x) {
      printf("This is not a valid Ru/Sa/Em/FR/LG save file!\n");
      return -1;
    }
  }

  // Check if save has enabled mistery gift
  switch (games) {
    case RUBY_AND_SAPPHIRE:  // not that it has wondercards...but let's see the code for it
      /*
          if ( (sav[0x3A9 + 0x1000 * sec[2] + currentSav]&0x10) == 0)
          {
              printf("Mistery Event is not enabled in savegame!\n");
              goto exit_app;
              return -2;
          }
          break;
      */

    case EMERALD:
      /*
          //Mistery Event (only really used by japanese)
          if ( (sav[0x405 + 0x1000 * sec[2] + currentSav]&0x10) == 0)
          {
              printf("Mistery Event is not enabled in savegame!\n");
              goto exit_app;
              return -2;
          }
      */
      // Mistery Gift
      if ((sav[0x40B + 0x1000 * sec[2] + currentSav] & 0x8) == 0) {
        printf("Mistery Gift is not enabled in savegame!\n");
        return -3;
      }
      break;

    case FIRE_RED_AND_LEAF_GREEN:
      // Mistery Gift
      if ((sav[0x67 + 0x1000 * sec[2] + currentSav] & 0x2) == 0) {
        printf("Mistery Gift is not enabled in savegame!\n");
        return -3;
      }
      break;
  }

  // Inject WC
  if (language == JAPANESE) {
    memcpy(sav + (wc_offset + 0x1000 * sec[4] + currentSav), wc3,
           0x4 + 0xA4);  // checksum+WC
    memcpy(
        sav + ((wc_offset + 0x4 + 0xA4 + 0xA) + 0x1000 * sec[4] + currentSav),
        wc3 + 0x4 + 0xA4 + 0xA, 2);  // Icon
    memcpy(sav + (wc_script_offset + 0x1000 * sec[4] + currentSav),
           wc3 + 0x4 + 0xA4 + 0x28 + 0x28,
           1004);  // Script data (chk(4) + association(4) + script(996))
  } else {
    memcpy(sav + (wc_offset + 0x1000 * sec[4] + currentSav), wc3,
           0x4 + 0x14C);  // checksum+WC
    memcpy(
        sav + ((wc_offset + 0x4 + 0x14C + 0xA) + 0x1000 * sec[4] + currentSav),
        wc3 + 0x4 + 0x14C + 0xA, 2);  // Icon
    memcpy(sav + (wc_script_offset + 0x1000 * sec[4] + currentSav),
           wc3 + 0x4 + 0x14C + 0x28 + 0x28,
           1004);  // Script data (chk(4) + association(4) + script(996))
  }

  // Update section 4 checksums
  char *ptr = sav + (0x1000 * sec[4] + currentSav);
  int chk = Chksum(DATALEN, (int *)ptr);
  sav[(0xFF6 + 0x1000 * sec[4] + currentSav)] = chk & 0x000000FF;
  sav[(0xFF7 + 0x1000 * sec[4] + currentSav)] = (chk & 0x0000FF00) >> 8;
  printf("Updating savegame section 4 checksum...(%04X)...", chk);

  return 1;
}

int me_inject(char *sav, char *me3, SupportedGames games, Language language)
{
  if (me3 == NULL && language != 0 &&
      games != EMERALD)  // Only allow NULL card for emerald eon ticket JAP
    return -4;

  unsigned int currentSav = 0, sec[14] = {}, sec0, s0, sx, x;
  int me_offset = 0x0;

  switch (language) {
    case JAPANESE:
      switch (games) {
        case RUBY_AND_SAPPHIRE:
          me_offset = ME3_OFFSET_RS;
          break;
        case EMERALD:
          me_offset = ME3_OFFSET_E;
          break;
      }
      break;
    default:
      switch (games) {
        case RUBY_AND_SAPPHIRE:
          me_offset = ME3_OFFSET_RS;
          break;
        case EMERALD:
          me_offset = ME3_OFFSET_E;
          break;
      }
      break;
  }

  // check where the current save is
  currentSav = checkCurrentSav(sav);

  // get section locations in current save
  for (s0 = 0; s0 <= 13; s0++) {
    sec0 = sav[0x0FF4 + 0x1000 * s0 + currentSav];
    if (sec0 == 0x0) {
      for (sx = 0; sx <= 13; sx++) {
        if ((s0 + sx) <= 13) {
          sec[sx] = s0 + sx;
        } else {
          sec[sx] = s0 + sx - 14;
        }
      }
      break;
    }
  }

  // check if all sections were found
  for (x = 0; x <= 13; x++) {
    if (sav[0x0FF4 + 0x1000 * sec[x] + currentSav] != x) {
      printf("This is not a valid Ru/Sa/Em/FR/LG save file!\n");
      return -1;
    }
  }

  // Check if save has enabled mistery gift
  switch (games) {
    case RUBY_AND_SAPPHIRE:

      if ((sav[0x3A9 + 0x1000 * sec[2] + currentSav] & 0x10) == 0) {
        printf("Mistery Event is not enabled in savegame!\n");
        return -2;
      }
      break;

    case EMERALD:

      // Mistery Event (only really used by japanese)
      if ((sav[0x405 + 0x1000 * sec[2] + currentSav] & 0x10) == 0) {
        printf("Mistery Event is not enabled in savegame!\n");
        return -2;
      }
      /*
          //Mistery Gift
          if ( (sav[0x40B + 0x1000 * sec[2] + currentSav]&0x8) == 0)
          {
              printf("Mistery Gift is not enabled in savegame!\n");
              return -3;
          }
      */
      break;
  }

  if (games == EMERALD && me3 == NULL)  // Emerald Eon Ticket is an in-game event
  {
    // Enable flag
    sav[(0x49A + 0x1000 * sec[2] + currentSav)] |= 0x01;
    // Input distro item chk
    sav[(0xC94 + 0x1000 * sec[4] + currentSav)] = 0xAC;
    sav[(0xC95 + 0x1000 * sec[4] + currentSav)] = 0x0;
    sav[(0xC96 + 0x1000 * sec[4] + currentSav)] = 0x0;
    sav[(0xC97 + 0x1000 * sec[4] + currentSav)] = 0x0;
    // Input distro item
    sav[(0xC98 + 0x1000 * sec[4] + currentSav)] = 0x01;
    sav[(0xC99 + 0x1000 * sec[4] + currentSav)] = 0x97;
    sav[(0xC9A + 0x1000 * sec[4] + currentSav)] = 0x13;
    sav[(0xC9B + 0x1000 * sec[4] + currentSav)] = 0x01;

    // Update section 2 checksums
    char *ptr = sav + (0x1000 * sec[2] + currentSav);
    int chk = Chksum(DATALEN, (int *)ptr);
    sav[(0xFF6 + 0x1000 * sec[2] + currentSav)] = chk & 0x000000FF;
    sav[(0xFF7 + 0x1000 * sec[2] + currentSav)] = (chk & 0x0000FF00) >> 8;
    printf("Updating savegame section 2 checksum...(%04X)...", chk);

    // Update section 4 checksums
    ptr = sav + (0x1000 * sec[4] + currentSav);
    chk = Chksum(DATALEN, (int *)ptr);
    sav[(0xFF6 + 0x1000 * sec[4] + currentSav)] = chk & 0x000000FF;
    sav[(0xFF7 + 0x1000 * sec[4] + currentSav)] = (chk & 0x0000FF00) >> 8;
    printf("Updating savegame section 4 checksum...(%04X)...", chk);

  } else {
    // Inject Mistery Event
    if (language == JAPANESE) {
      memcpy(sav + (me_offset + 0x1000 * sec[4] + currentSav), me3,
             1012);  // Script data (chk(4) + association(4) + script(996)) +
                     // item data (8)
    } else {
      memcpy(sav + (me_offset + 0x1000 * sec[4] + currentSav), me3,
             1012);  // Script data (chk(4) + association(4) + script(996)) +
                     // item data (8)
    }

    // Update section 4 checksums
    char *ptr = sav + (0x1000 * sec[4] + currentSav);
    int chk = Chksum(DATALEN, (int *)ptr);
    sav[(0xFF6 + 0x1000 * sec[4] + currentSav)] = chk & 0x000000FF;
    sav[(0xFF7 + 0x1000 * sec[4] + currentSav)] = (chk & 0x0000FF00) >> 8;
    printf("Updating savegame section 4 checksum...(%04X)...", chk);
  }

  return 1;
}
