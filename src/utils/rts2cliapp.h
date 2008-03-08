/* 
 * Command Line Interface application sceleton.
 * Copyright (C) 2003-2007 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __RTS2_CLIAPP__
#define __RTS2_CLIAPP__

#include "rts2app.h"

#include <libnova/libnova.h>

class Rts2CliApp:public Rts2App
{
	protected:
		virtual int doProcessing () = 0;
		virtual void afterProcessing ();

	public:
		Rts2CliApp (int in_argc, char **in_argv);

		virtual int run ();

		/**
		 * Parses and initialize tm structure from char.
		 *
		 * String can contain either date, in that case it will be converted to night
		 * starting on that date, or full date with time (hour, hour:min, or hour:min:sec).
		 *
		 * @return -1 on error, 0 on succes
		 */
		int parseDate (const char *in_date, struct ln_date *out_time);
};
#endif							 /* !__RTS2_CLIAPP__ */
