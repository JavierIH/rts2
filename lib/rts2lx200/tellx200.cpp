/* 
 * LX200 protocol abstract class.
 * Copyright (C) 2009-2010 Markus Wildi
 * Copyright (C) 2010 Petr Kubanek, Institute of Physics
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

#include "rts2lx200/tellx200.h"
#include "rts2lx200/hms.h"

#define OPT_CONN_DEBUG     OPT_LOCAL + 2000

using namespace rts2teld;

TelLX200::TelLX200 (int in_argc, char **in_argv):Telescope (in_argc,in_argv)
{
	device_file = "/dev/ttyS0";
	connDebug = false;

	createValue (localTime, "LOCATIME", "telescope local time", true, RTS2_DT_RA);

	addOption ('f', NULL, 1, "serial device file (default to /dev/ttyS0");
	addOption (OPT_CONN_DEBUG, "conndebug", 0, "record debug log of messages on connections");
}

TelLX200::~TelLX200 (void)
{
	delete serConn;
}

int TelLX200::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'f':
			device_file = optarg;
			break;
		case OPT_CONN_DEBUG:
			connDebug = true;
			break;
		default:
			return Telescope::processOption (in_opt);
	}
	return 0;
}

int TelLX200::init ()
{
	int ret;
	ret = Telescope::init ();
  
	if (ret)
		return ret;

	serConn = new rts2core::ConnSerial (device_file, this, rts2core::BS9600, rts2core::C8, rts2core::NONE, 5, 5);
	if (connDebug == true)
		serConn->setDebug (true);
	ret = serConn->init ();
	if (ret)
		return -1;
	serConn->flushPortIO ();

	return 0;
}

int TelLX200::tel_read_hms (double *hmsptr, const char *command, bool allowZ)
{
	char wbuf[256];
	int len;
	if ((len = serConn->writeRead (command, strlen (command), wbuf, 200, '#')) < 0)
		return -1;

	wbuf[len - 1] = '\0';
	if (allowZ && wbuf[0] == 'Z')
		*hmsptr = hmstod (wbuf + 1);
	else
		*hmsptr = hmstod (wbuf);
	if (isnan (*hmsptr))
	{
		logStream (MESSAGE_ERROR) << "invalid character for HMS: " << wbuf << sendLog;
		serConn->flushPortIO ();
		return -1;
	}
	return 0;
}

int TelLX200::tel_read_ra ()
{
	double new_ra;
	if (tel_read_hms (&new_ra, "#:GR#"))
		return -1;

	setTelRa (fmod((new_ra * 15.0) + 360., 360.));
	return 0;
}

int TelLX200::tel_read_dec ()
{
	double t_telDec;
	if (tel_read_hms (&t_telDec, "#:GD#"))
		return -1;
	
	setTelDec (fmod( t_telDec,  90.));
	return 0;
}

int TelLX200::tel_read_altitude ()
{
	double new_altitude;
	if (tel_read_hms (&new_altitude, "#:GA#"))
		return -1;
	telAltAz->setAlt(new_altitude);
	return 0;
}

int TelLX200::tel_read_azimuth ()
{
	double new_azimuth;
	if (tel_read_hms (&new_azimuth, "#:GZ#"))
		return -1;
	telAltAz->setAz(new_azimuth);
	return 0;
}

int TelLX200::tel_read_local_time ()
{
	double new_local_time ;
	if (tel_read_hms (&new_local_time, "#:GL#", true))
		return -1;
	localTime->setValueDouble (new_local_time * 15.);
	return 0;
}

int TelLX200::tel_read_sidereal_time ()
{
	double new_sidereal_time;
	if (tel_read_hms (&new_sidereal_time, "#:GS#"))
		return -1;
  	lst->setValueDouble (new_sidereal_time *15.);
	return 0;
}

int TelLX200::tel_read_latitude ()
{
	double new_latitude;
	if (tel_read_hms (&new_latitude, "#:Gt#"))
		return -1;
	telLatitude->setValueDouble (new_latitude);
	return 0;
}

int TelLX200::tel_read_longitude ()
{
	double new_longitude;
	if (tel_read_hms (&new_longitude, "#:Gg#") < 0 )
		return -1;
  	telLongitude->setValueDouble (-1 * new_longitude);
	return 0;
}

int TelLX200::tel_rep_write (char *command)
{
	int count;
	char retstr;
	for (count = 0; count < 3; count++)
	{
		if (serConn->writeRead (command, strlen (command), &retstr, 1) < 0)
			return -1;
		if (retstr == '1')
			return 0;
		sleep (1);
	}
	logStream (MESSAGE_ERROR) << "unsucessful write due to incorrect return ('1' not received)." << sendLog;
	return -1;
}

int TelLX200::tel_write_ra (double ra)
{
	char command[32];
	int h, m, s;
	if (ra < 0 || ra > 360.0)
		return -1;
	ra = ra / 15.;
	dtoints (ra, &h, &m, &s);
	// Astro-Physics format
	if (snprintf (command, sizeof( command), ":Sr %02d:%02d:%02d#", h, m, s) < 0)
		return -1;
	return tel_rep_write (command);
}

int TelLX200::tel_write_dec (double dec)
{
	char command[15];
	struct ln_dms dh;
	char sign = '+';
	if (dec < 0)
	{
		sign = '-';
		dec = -1 * dec;
	}
	ln_deg_to_dms (dec, &dh);
	if (snprintf (command, 15, ":Sd%c%02d*%02d:%02.0f#", sign, dh.degrees, dh.minutes, dh.seconds) < 0)
		return -1;
	return tel_rep_write (command);
}

int TelLX200::tel_write_altitude (double alt)
{
	char command[15];
	struct ln_dms dh;
	char sign = '+';
	if (alt < 0)
	{
		sign = '-';
		alt = -1 * alt;
	}
	ln_deg_to_dms (alt, &dh);
	if (snprintf (command, 15, ":Sa%c%02d*%02d:%02.0f#", sign, dh.degrees, dh.minutes, dh.seconds) < 0)
		return -1;
	return tel_rep_write (command);
}

int TelLX200::tel_write_azimuth (double az)
{
	char command[16];
	struct ln_dms dh;
	ln_deg_to_dms (az, &dh);
	if (snprintf (command, 16, ":Sz %03d*%02d:%02.0f#", dh.degrees, dh.minutes, dh.seconds) < 0)
		return -1;
	return tel_rep_write (command);
}

int TelLX200::tel_set_slew_rate (char new_rate)
{
	char command[6];
	sprintf (command, ":RS%c#", new_rate); // slew
	return serConn->writePort (command, 5);
}

int TelLX200::tel_start_move (char direction)
{
	char command[6];
	sprintf (command, "#:M%c#", direction);
	return serConn->writePort (command, 5) == 0 ? 0 : -1;
}

int TelLX200::tel_stop_move (char direction)
{
	char command[6];
	sprintf (command, "#:Q%c#", direction);
	return serConn->writePort (command, 5) == 0 ? 0 : -1;
}