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

#ifndef LOGF_H_INCLUDED
#define LOGF_H_INCLUDED

void logf(const char * const fmt, ...);
#define logfi(...) {logf_printindent(); logf(__VA_ARGS__);}
#define logf_abort(...) {logfi(__VA_ARGS__); logf_flushabort();}
void logf_flushabort(void);
void logf_setstdouton(const int val);
void logf_setmute(const int val);
void logf_openlogfile(const char * const filename);
void logf_closelogfile(void);

void logf_printindent(void);
void logf_addindent(void);
void logf_delindent(void);

void logf_hexdump(const char * const name, const void * const vptr, unsigned size);
void _flogf_hexdump(const char * const name, const void __far * const vptr, unsigned size);

void logf_current_segments(void);
void logf_ivt_registered(void);


#endif /* #ifndef LOGF_H_INCLUDED */
