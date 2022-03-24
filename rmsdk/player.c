/*
 *  Copyright (C) 2022 Jon Dennis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdint.h>

#include "logf.h"
#include "reelmagc.h"

static void
play_file(const char * const filename) {
  rm_handle_t handle;
  uint16_t    stream_types;

  /* reset and init rm global stuff */
  rm_reset();
  rm_set_zorder(RM_GLOBAL, RMZORDER_BEHINDVGA);

  /* open our handle */
  handle = rm_open(filename, RMOPEN_FILE);
  if (handle == RM_HANDLE_INVALID) {
    logf_abort("Failed to open file: %s\n", filename);
  }

  stream_types = rm_get_stream_types(handle);
  if (!RMSTREAMTYPES_OK(stream_types)) {
    logf_abort("Error reading file: %s\n", filename);
  }
  logf("Streams Detected:%s%s\n",
    RMSTREAMTYPES_HASVIDEO(stream_types) ?  " video" : "",
    RMSTREAMTYPES_HASAUDIO(stream_types) ?  " audio" : "");
    
  /* play the handle */
  rm_play(handle, RMPLAY_PAUSE_ON_FINISH);

  /* wait indefinitely for it to finish */
  rm_waitplay(handle, 0);

  /* close the handle */
  rm_close(handle);
}


int
main(int argc, char *argv[]) {
  logf_openlogfile("player.log");

  if (argc != 2) {
    logf("Usage: PLAYER.EXE MPEG_FILE\n");
    return 1;
  }

  if (!rm_init()) logf_abort("Failed to initialize ReelMagic");
  play_file(argv[1]);
  logf_closelogfile();
  return 0;
}


