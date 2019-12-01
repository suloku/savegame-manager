/*
 * savegame_manager: a tool to backup and restore savegames from Nintendo
 *  DS cartridges. Nintendo DS and all derivative names are trademarks
 *  by Nintendo. EZFlash 3-in-1 is a trademark by EZFlash.
 *
 * main.cpp: main file
 *
 * Copyright (C) Pokedoc (2010)
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <dirent.h>
#include <errno.h>
#include <fat.h>
#include <math.h>
#include <nds.h>
#include <nds/arm9/console.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <string.h>
#include <sys/dir.h>

#include <algorithm>

#include "display.h"
#include "dsCard.h"
#include "dsi.h"
#include "fileselect.h"
#include "gba.h"
#include "globals.h"
#include "hardware.h"
#include "libini.h"
#include "me.h"
#include "strings.h"

using std::max;

char bootdir[256] = "/";

#define LIBNDS_VER \
  ((_LIBNDS_MAJOR_ << 16) | (_LIBNDS_MINOR_ << 8) | (_LIBNDS_PATCH_))

// needs libnds version 1.5.4, for the SDHC driver on the DSi.
#if (LIBNDS_VER < 0x00010504)
#error "Your libnds version is outdated! Please use 1.5.4 or higher!"
#endif

#define REBOOT_WIFI

enum main_mode { None, Backup, Restore, Erase };

enum main_mode select_main_screen_option(int* cursor) {
  // touch screen selection
  touchPosition touchXY;
  touchRead(&touchXY);

  if ((touchXY.py > 8 * 0) && (touchXY.py < 8 * 8)) {
    return Backup;
  }

  if ((touchXY.py > 8 * 8) && (touchXY.py < 8 * 16)) {
    return Restore;
  }

  if ((touchXY.py > 8 * 16) && (touchXY.py < 8 * 24)) {
    return Erase;
  }

  // button based selection
  scanKeys();
  uint32 keys = keysDown();
  if (keys & KEY_DOWN) {
    (*cursor)++;
  }
  if (keys & KEY_UP) {
    (*cursor)--;
  }
  if (*cursor < 0) {
    *cursor = 2;
  }
  if (*cursor > 2) {
    *cursor = 0;
  }

  if (keys & KEY_A) {
    switch (*cursor) {
      case 0:
        return Backup;
      case 1:
        return Restore;
      case 2:
        return Erase;
    }
  }

  return None;
}

// ============================================================================
void mode_dsi() {
  // TODO: test is SD card is present!

  // use 3in1 to buffer data
  displayStateF(STR_EMPTY);
  displayPrintUpper(true);

  // DSi mode, does nothing at the moment
  displayMessageF(STR_BOOT_MODE_UNSUPPORTED);

  int cursor_position = 0;
  while (1) {
    swiWaitForVBlank();
    enum main_mode mode = select_main_screen_option(&cursor_position);
    displayPrintLower(cursor_position);

    if (mode == Backup) {
      hwBackupDSi();
    }

    if (mode == Restore) {
      hwRestoreDSi();
    }

    if (mode == Erase) {
      // if ( swap_cart(true) ) {
      //  displayPrintUpper();
      //  hwErase();
      //}
    }
  }
}

void mode_slot2() {
  // use slot2 DLDI device to store data
  displayStateF(STR_EMPTY);
  displayPrintUpper(true);

  int cursor_position = 0;
  while (1) {
    swiWaitForVBlank();
    enum main_mode mode = select_main_screen_option(&cursor_position);
    displayPrintLower(cursor_position);

    if (mode == Backup) {
      hwBackupSlot2();
    }

    if (mode == Restore) {
      hwRestoreSlot2();
    }

    if (mode == Erase) {
      if (swap_cart(true)) {
        displayPrintUpper();
        hwErase();
      }
    }
  }
}

void mode_3in1() {
  displayPrintUpper(true);

  dsCardData data2;
  uint32 ime = hwGrab3in1();
  ReadNorFlash(data, 0, 0x8000);
  hwRelease3in1(ime);
  memcpy(&data2, &data[0x1000], sizeof(data2));
  if ((data2.data[0] == RS_BACKUP) && (data2.data[1] == 0) &&
      (data2.data[3] == 0xffff00ff)) {
    uint32 size = data2.data[2];
    char name[13];
    memcpy(&name[0], &data2.name[0], 12);
    name[12] = 0;
    displayMessageF(
        STR_HW_3IN1_CLEAR_FLAG);  // lower screen is used for file browser
    hwFormatNor(0, 1);            // clear reboot flag
    hwDump3in1(size, name);
  }
  displayMessageF(STR_EMPTY);
  displayProgressBar(0, 0);

  int cursor_position = 0;
  while (1) {
    swiWaitForVBlank();
    enum main_mode mode = select_main_screen_option(&cursor_position);
    displayPrintLower(cursor_position);

    if (mode == Backup) {
      hwBackup3in1();
    }

    if (mode == Restore) {
      hwRestore3in1();
    }

    if (mode == Erase) {
      if (swap_cart(true)) {
        displayPrintUpper();
        hwErase();
      }
    }
  }
}

void mode_gba() {
  // This function takes some time, since it needs to parse the entire GBA
  // module
  //  for a magic string. It is only called when truly necessary. (i.e. once for
  //  now)

reload_cart:

  u8 gbatype = gbaGetSaveType();

  // use 3in1 to buffer data
  displayStateF(STR_EMPTY);
  gbatype = gbaGetSaveType();
  displayPrintUpper(true);
  displayLoadingCart();

  // Get game/language from GAME ID
  int language = 0;
  int game = 0;
  int maxoptions = 0;

  char lang_[8];
  sprintf(lang_, "%.4s", (char*)0x080000ac);

  switch (lang_[3])  // JAP
  {
    case 0x4A:  // JAP
      language = 1;
      break;
    case 0x45:  // ENG
      language = 2;
      break;
    case 0x46:  // FRE
      language = 3;
      break;
    case 0x49:  // ITA
      language = 4;
      break;
    case 0x44:  // GER
      language = 5;
      break;
    case 0x53:  // ESP
      language = 7;
      break;
    default:
      language = 2;
      break;
  }

  if (strncmp("AXV", (char*)0x080000ac, 3) == 0 ||
      strncmp("AXP", (char*)0x080000ac, 3) == 0)  // R S
  {
    game = 0;
  } else if (strncmp("BPE", (char*)0x080000ac, 3) == 0)  // E
  {
    game = 1;
  } else if (strncmp("BPR", (char*)0x080000ac, 3) == 0 ||
             strncmp("BPG", (char*)0x080000ac, 3) == 0)  // FR LG
  {
    game = 2;
  } else {
    game = 3;  // Unknown
  }

  if (game != 3) {
    switch (language) {
      case 1:
        switch (game) {
          case 0:
            maxoptions = 0;
            break;
          case 1:
            maxoptions = 2;
            break;
          case 2:
            maxoptions = 1;
            break;
        }
        break;
      case 2:
        switch (game) {
          case 0:
            maxoptions = 1;
            break;
          case 1:
            maxoptions = 2;
            break;
          case 2:
            maxoptions = 1;
            break;
        }
        break;
      default:
        switch (game) {
          case 0:
            maxoptions = 0;
            break;
          case 1:
            maxoptions = 1;
            break;
          case 2:
            maxoptions = 1;
            break;
        }
        break;
    }
  }

  int cursor_position = 0;
  while (1) {
    swiWaitForVBlank();
    // enum main_mode mode = select_main_screen_option(&cursor_position);
    // displayPrintLower( cursor_position );

    if (game != 3) {
      displayPrintTickets(cursor_position, game, language);

      scanKeys();
      uint32 keys = keysDown();
      if (keys & KEY_DOWN) {
        cursor_position++;
      }
      if (keys & KEY_UP) {
        cursor_position--;
      }
      if (cursor_position < 0) {
        cursor_position = maxoptions;
      }
      if (cursor_position > maxoptions) {
        cursor_position = 0;
      }

      if (keys & KEY_START) {
        while (1) {
          swiWaitForVBlank();
          displayChangeCart(0);
          scanKeys();
          uint32 keys = keysDown();
          if (keys & KEY_START) {
            displayLoadingCart();
            goto reload_cart;
          }
        }
      }

      if (keys & KEY_A) {
        displayPrintUpper();

        switch (language) {
          case 1:
            switch (game) {
              case 0:
                GBA_read_inject_restore(gbatype, eon_ticket_jap, game,
                                        language);
                break;
              case 1:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, NULL, game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_E_jap, game,
                                            language);
                    break;
                  case 2:
                    GBA_read_inject_restore(gbatype, old_map_jap, game,
                                            language);
                    break;
                }
                break;
              case 2:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, aurora_ticket_FRLG_jap,
                                            game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_FRLG_jap,
                                            game, language);
                    break;
                }
                break;
            }
            break;
          case 2:
            switch (game) {
              case 0:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, eon_ticket_card_eng, game,
                                            language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, eon_ticket_ninti_eng, game,
                                            language);
                    break;
                }
                break;
              case 1:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, aurora_ticket_E_eng, game,
                                            language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_E_eng, game,
                                            language);
                    break;
                  case 2:
                    GBA_read_inject_restore(
                        gbatype, unofficial_old_sea_map_E_eng, game, language);
                    break;
                }
                break;
              case 2:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, aurora_ticket_FRLG_eng,
                                            game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_FRLG_eng,
                                            game, language);
                    break;
                }
                break;
            }
            break;
          case 3:
            switch (game) {
              case 0:
                GBA_read_inject_restore(gbatype, eon_ticket_ninti_fre, game,
                                        language);
                break;
              case 1:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, aurora_ticket_E_ninti_fre,
                                            game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_E_eng, game,
                                            language);
                    break;
                }
                break;
              case 2:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(
                        gbatype, aurora_ticket_FRLG_ninti_fre, game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_FRLG_eng,
                                            game, language);
                    break;
                }
                break;
            }
            break;
          case 4:
            switch (game) {
              case 0:
                GBA_read_inject_restore(gbatype, eon_ticket_ninti_ita, game,
                                        language);
                break;
              case 1:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, aurora_ticket_E_ninti_ita,
                                            game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_E_eng, game,
                                            language);
                    break;
                }
                break;
              case 2:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(
                        gbatype, aurora_ticket_FRLG_ninti_ita, game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_FRLG_eng,
                                            game, language);
                    break;
                }
                break;
            }
            break;
          case 5:
            switch (game) {
              case 0:
                GBA_read_inject_restore(gbatype, eon_ticket_ninti_ger, game,
                                        language);
                break;
              case 1:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, aurora_ticket_E_ninti_ger,
                                            game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_E_eng, game,
                                            language);
                    break;
                }
                break;
              case 2:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(
                        gbatype, aurora_ticket_FRLG_ninti_ger, game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_FRLG_eng,
                                            game, language);
                    break;
                }
                break;
            }
            break;
          case 7:
            switch (game) {
              case 0:
                GBA_read_inject_restore(gbatype, eon_ticket_ninti_esp, game,
                                        language);
                break;
              case 1:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(gbatype, aurora_ticket_E_ninti_esp,
                                            game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_E_eng, game,
                                            language);
                    break;
                }
                break;
              case 2:
                switch (cursor_position) {
                  case 0:
                    GBA_read_inject_restore(
                        gbatype, aurora_ticket_FRLG_ninti_esp, game, language);
                    break;
                  case 1:
                    GBA_read_inject_restore(gbatype, mystic_ticket_FRLG_eng,
                                            game, language);
                    break;
                }
                break;
            }
            break;
        }

        /*
        if (game == 0)
            GBA_read_inject_restore(gbatype, eon_ticket_ita, game, language);
        else if (game == 2)
            GBA_read_inject_restore(gbatype, aurora_ticket_eng, game, language);
        */
      }
    } else {
      while (1) {
        swiWaitForVBlank();
        displayChangeCart(1);
        scanKeys();
        uint32 keys = keysDown();
        if (keys & KEY_START) {
          displayLoadingCart();
          goto reload_cart;
        }
      }
    }
  }
}

void mode_wifi() {
  // displayStateF(STR_EMPTY);
  displayPrintUpper(true);

  int cursor_position = 0;
  while (1) {
    swiWaitForVBlank();
    enum main_mode mode = select_main_screen_option(&cursor_position);
    displayPrintLower(cursor_position);

    if (mode == Backup) {
      hwBackupFTP();
#ifdef REBOOT_WIFI
      displayMessage2F(STR_HW_PLEASE_REBOOT);
      while (1)
        ;
#endif
    }

    if (mode == Restore) {
      hwRestoreFTP();
#ifdef REBOOT_WIFI
      displayMessage2F(STR_HW_PLEASE_REBOOT);
      while (1)
        ;
#endif
    }

    if (mode == Erase) {
      if (swap_cart(true)) {
        displayPrintUpper();
        hwErase();
      }
    }
  }
}

void mode_dlp() {
  // use non-flash card based exploits (download play or Sudoku?). untested, and
  // does not work yet!
  displayStateF(STR_EMPTY);
  displayPrintUpper();

  // DSi mode, does nothing at the moment
  displayMessage2F(STR_STR,
                   "I did not expect that you can trigger this mode at all!");
  while (1)
    ;

  int cursor_position = 0;
  while (1) {
    swiWaitForVBlank();
    enum main_mode mode = select_main_screen_option(&cursor_position);
    displayPrintLower(cursor_position);

    if (mode == Backup) {
      hwBackupFTP(true);
    }

    if (mode == Restore) {
      hwRestoreFTP(true);
    }

    if (mode == Erase) {
      displayPrintUpper();
      hwErase();
      displayWarning2F(STR_HW_DID_DELETE);
      while (1)
        ;
    }
  }
}

bool loadIniFile(char* path) {
  // Don't try to load ini file in DLP mode, we need to get our options from a
  // different source.
  //  Still trying to figure something out...
  if (mode == 5) return true;

  ini_fd_t ini = 0;

  // load ini file...
  char inipath[256];
  if (path) {
    char* last = strrchr(path, '/');
    int len = (last - path) + 1;
    strncpy(bootdir, path, len);
    sprintf(inipath, "%s/savegame_manager.ini", bootdir);
    if (fileExists(inipath)) ini = ini_open(inipath, "r", "");
  }
  if (!ini) {
    sprintf(inipath, "/savegame_manager.ini");
    if (fileExists(inipath)) ini = ini_open(inipath, "r", "");
  }
  if (!ini) {
    // displayWarning2F(STR_BOOT_NO_INI);
    iprintf("could not find ini file!");
    while (1)
      ;
  }

#ifdef DEBUG
  iprintf("Found and parsed INI file.\n");
#endif

  ini_locateHeading(ini, "");
  ini_locateKey(ini, "ftp_ip");
  ini_readString(ini, ftp_ip, 16);
  ini_locateKey(ini, "ftp_user");
  ini_readString(ini, ftp_user, 64);
  ini_locateKey(ini, "ftp_pass");
  ini_readString(ini, ftp_pass, 64);
  ini_locateKey(ini, "ftp_port");
  ini_readInt(ini, &ftp_port);

  ini_locateKey(ini, "ir_delay");
  ini_readInt(ini, &ir_delay);
  ir_delay = max(ir_delay, 1000);

  if (ini_locateKey(ini, "slot2")) ini_readInt(ini, &slot2);

  // load additional Flash chip signatures (JEDEC IDs)
  ini_locateHeading(ini, "new chips");
  for (int i = 0; i < EXTRA_ARRAY_SIZE; i++) {
    int tmp;
    sprintf(txt, "%i-id", i);
    ini_locateKey(ini, txt);
    ini_readString(ini, txt, 256);
    sscanf(txt, "%x", &tmp);
    extra_id[i] = (u32)tmp;
    //
    sprintf(txt, "%i-size", i);
    ini_locateKey(ini, txt);
    ini_readInt(ini, &tmp);
    extra_size[i] = tmp;
  }

  txt[0] = 0;
  ini_locateHeading(ini, "");
  ini_locateKey(ini, "language");
  ini_readString(ini, txt, 256);

  ini_close(ini);

  // delete temp file (which is a remnant of inilib)
  remove("/tmpfile");

  return true;
}

// This is a new attempt to reliably identify if the flash card has argv support
// or not,
//  including R4 clones which may have *bad* argv support.
bool has_argv(int argc, char* argv[]) {
  // no argv support
  if (argc == 0) return false;

  // bad argv support: if argc is larger than unity, your flash card can't even
  // do
  //  a proper negative indicator. let us test for 2 argv arguments, just to be
  //  safe.
  if (argc > 2) return false;

  /*
  // test if argv[0] pointer is valid (scan main memory and WRAM)
  if ((argv[0] < 0x02000000) || (argv[0] >= 0x04000000))
      return false;
  */

  // test if argv[0] contains a valid string
  char* arg0 = argv[0];
  // test for possible "/path", "fat*:/path" and "sd:/*".
  if (*arg0 == 'f')
    if (!strncasecmp(argv[0], "fat:/", 5) ||
        !strncasecmp(argv[0], "fat1:/", 6) ||
        !strncasecmp(argv[0], "fat2:/", 6))
      return true;

  if (*arg0 == 's')
    if (!strncasecmp(argv[0], "sd:/", 4)) {
      sdslot = true;  // for sudokuhax
      return true;
    }

  if (*arg0 == '/') return true;

  return false;
}

int main(int argc, char* argv[]) {
  sysSetBusOwners(true, true);

  // Init the screens
  displayInit();

  /*
      // Init DLDI (file system driver)
      sysSetBusOwners(true, true);
      int fat = fatInitDefault();
      if (fat == 0) {
          //displayWarning2F(STR_BOOT_DLDI_ERROR);
          iprintf("DLDI error");
          while (1);
      }
  #ifdef DEBUG
      iprintf("Found DLDI: %s\n", io_dldi_data->friendlyName);
  #endif
  */
  // detect hardware
  mode = hwDetect();
  /*
      // Load the ini file with the FTP settings and more options
      for (int i = 0; i < EXTRA_ARRAY_SIZE; i++) {
          extra_id[i] = 0xff000000;
          extra_size[i] = 0;
      }

  #ifdef DEBUG
      iprintf("Loading INI file\n");
  #endif
      if (has_argv(argc, argv)) {
  #ifdef DEBUG
          iprintf("argv folder: %s\n", argv[0]);
  #endif
          loadIniFile(argv[0]);
      } else {
  #ifdef DEBUG
          iprintf("Bad/no argv folder!\n", argv[0]);
  #endif
          loadIniFile(0);
      }
  #ifdef DEBUG
      iprintf("Done!\n");
  #endif
  */
  if (slot2 > 0) mode = 4;

  // load strings
  // "txt" is used as a temp buffer for the language file name, returned by
  // hwDetect
  stringsLoadFile(txt);

  // prepare the global data buffer
  data = (u8*)malloc(size_buf);
  // On my Cyclops, no 2MB buffer is available after loading strings! bad memory
  // management?
  //  Anyway, we can't do anything except for the following (which restricts the
  //  usefulness of FTP safe mode)
#ifdef DEBUG
  iprintf("Allocating memory buffer.\n");
#endif
  while (data == NULL) {
    size_buf >>= 1;
    data = (u8*)malloc(size_buf);
  }
#ifdef DEBUG
  iprintf("Done! We now have %x.\n", size_buf);
#endif

  // Init the screens
  displayTitle();
  mode = 1;
  mode_gba();
  /*
  // okay, we got our HW identified; now branch to the corresponding main
  function/event handler switch (mode) { case 1: mode_gba(); break; case 2:
          mode_3in1();
          break;
      case 3:
          mode_dsi();
          break;
      case 4:
          mode_slot2();
          break;
      case 5:
          mode_dlp();
          break;
      default:
          mode_wifi();
  }
  */
  return 0;
}
