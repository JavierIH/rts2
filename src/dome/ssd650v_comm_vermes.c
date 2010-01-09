/*
 * SSD650v inverter communication
 * Copyright (C) 2010 Markus Wildi, version for RTS2
 * Copyright (C) 2009 Lukas Zimmermann, Basel, Switzerland
 *
 *
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Or visit http://www.gnu.org/licenses/gpl.html.

 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sys/time.h>

#endif
#include <math.h>

#include "vermes.h"
#include "ssd650v_comm_vermes.h"
#include "bisync_vermes.h"
#include "serial_vermes.h"
#include "util_vermes.h"

extern int debug;

typedef enum TAG_TYPES {
  TAGT_BOOL,
  TAGT_REAL,
  TAGT_INT,
  TAGT_WORD,
  TAGT_ENUM,
  TAGT_NBR
} tag_types_t;

typedef enum CMD_FORMATS {
  CMDF_MNEMO,
  CMDF_TAGNBR
} cmd_formats_t;

typedef enum PARAM_ACCESS {
  PARAM_RO,
  PARAM_WO,
  PARAM_RW
} param_access_t;

#define TRUE 1
#define FALSE 0

typedef unsigned char BOOLEAN;

/* structure holding all relevant information on a certain SSD650V command */
typedef struct tagdata {
  char * cmd_description;
  param_access_t param_perm;
  cmd_formats_t cmd_format;
  union {
    char mnemo[3];
    int tag;
  } command;
  tag_types_t data_type;
  union {
    BOOLEAN b; 
    float f;
    unsigned int i;
    unsigned char s;
    unsigned int tag_nbr;
  } data;
  unsigned int special_delay; /* a value != 0 indicates that this command needs
                               * more time than usual to complete,
                               * i.e. `special_delay' in milliseconds */
} tagdata_t;

tagdata_t load_settings_tseq[] = {
  {
    "Reset SSD650V",
    PARAM_WO,
    CMDF_MNEMO,
    {"!1"},
    TAGT_WORD,
    {(BOOLEAN)0x7777},
    1000
  },
  {
    "Restore configuration from non-volatile memory",
    PARAM_WO,
    CMDF_MNEMO,
    {"!1"},
    TAGT_WORD,
    {(BOOLEAN)0x0101},
    3000
  },
  {
    "Motor speed setpoint",
    PARAM_RW,
    CMDF_TAGNBR,
//    {(char)247},
    {""},
    TAGT_REAL,
    {100.0},
    0
  },
  {
    "Ramp accel time",
    PARAM_RW,
    CMDF_TAGNBR,
//    {(char)258},
    {""},
    TAGT_REAL,
    {5.0},
    0
  },
  {
    "Ramp decel time",
    PARAM_RW,
    CMDF_TAGNBR,
//    {(char)259},
    {""},
    TAGT_REAL,
    {2.0},
    0
  },
};
/******************************************************************************
 * motor_on(...)
 * Starts motor at setpoint.
 * 
 * Args:
 * Return:
 * state
 *****************************************************************************/
int motor_on()
{
  motor_state.start= SSD650V_ON ;
  motor_state.stop = SSD650V_OFF ;
  return motor_run_switch_state() ;
}
/******************************************************************************
 * motor_off(...)
 * Stops motor.
 * 
 * Args:
 * Return:
 * state
 *****************************************************************************/
int motor_off()
{
  motor_state.start= SSD650V_OFF ;
  motor_state.stop = SSD650V_ON ;
  return motor_run_switch_state() ;
}

/******************************************************************************
 * get_setpoint(...)
 * Reyrieves the motor speed setpoint to the SSD650V.
 * 
 * Args:
 * Return:
 *   a float representing the absolute value of the current setpoint
 *   or NaN if an error occured.
 *****************************************************************************/
float
get_setpoint()
{
  return SSD_qry_setpoint(ser_dev) ;
}

/******************************************************************************
 * set_setpoint(...)
 * Transmits the motor speed setpoint to the SSD650V.
 * 
 * Args:
 *   setpoint:
 *   direction: 0: don't change, 1: clockwise, -1: counter clockwise
 * Return:
 *   BISYNC_ERROR status, 0 when all ok.
 *****************************************************************************/
int
set_setpoint(float setpoint)
{
  char data[8];
  float snd_setpoint = setpoint;

  if (isnan(setpoint)) {
    indi_log(ILOG_WARNING, "Don't set setpoint to NaN!");
    return BISYNC_ERR;
  }

  if ( fabs( 100) > 100.) {
    indi_log(ILOG_WARNING, "Don't set setpoint to larger than 100.%");
    return BISYNC_ERR;
  }

  float ssd_setp = SSD_qry_real(ser_dev, 247);;
  if (isnan(ssd_setp)) {
    indi_log(ILOG_WARNING, "Failure querying setpoint");
    return BISYNC_ERR;
  }
  snprintf(data, 8, "%3.1f", snd_setpoint);
  return SSD_set_tag(ser_dev, 247, data);
}

/******************************************************************************
 * qry_mot_cmd()
 * Queries the motor command parameter (tag 273) and signals the state.
 *****************************************************************************/

// wildi ToDo: integrate me
int
qry_mot_cmd()
{
  char * id_str;
  int cmd_stat = SSD_qry_hexword(ser_dev, 273);
  if (cmd_stat >= 0) {
    asprintf(&id_str, "0x%04X", cmd_stat);
/*     IUSaveText(&SSD_device_infoTP.tp[3], id_str); */
    free(id_str);
    if (cmd_stat != mot_cmd_status) {
/*       SSD_MotorCmdLP.s = IPS_OK; */
      mot_cmd_status = cmd_stat;
    }
  } else {
    mot_cmd_status = -1;
/*     SSD_device_infoTP.s = IPS_ALERT; */
  }
  return cmd_stat;
}

/******************************************************************************
 * qry_mot_status()
 * Queries the motor sequencing status (tag 272) and signals the state.
 *****************************************************************************/
int
qry_mot_status()
{
  char * id_str;
  int cmd_stat = SSD_qry_hexword(ser_dev, 272);
  if (cmd_stat >= 0) {
    asprintf(&id_str, "0x%04X", cmd_stat);
/*     IUSaveText(&SSD_device_infoTP.tp[2], id_str); */
    free(id_str);
    if (cmd_stat != inverter_seq_status) {
/*       SSD_MotorSeqLP.lp[0].s = cmd_stat & 0x0001 ? IPS_OK : IPS_IDLE; */
/*       SSD_MotorSeqLP.lp[1].s = cmd_stat & 0x0002 ? IPS_OK : IPS_IDLE; */
/*       SSD_MotorSeqLP.lp[2].s = cmd_stat & 0x0004 ? IPS_OK : IPS_IDLE; */
/*       SSD_MotorSeqLP.lp[3].s = cmd_stat & 0x0008 ? IPS_OK : IPS_IDLE; */
/*       SSD_MotorSeqLP.lp[4].s = cmd_stat & 0x0010 ? IPS_IDLE : IPS_OK; */
/*       SSD_MotorSeqLP.lp[5].s = cmd_stat & 0x0020 ? IPS_IDLE : IPS_OK; */
/*       SSD_MotorSeqLP.lp[6].s = cmd_stat & 0x0040 ? IPS_OK : IPS_IDLE; */
/*       SSD_MotorSeqLP.lp[7].s = cmd_stat & 0x0200 ? IPS_OK : IPS_IDLE; */
/*       SSD_MotorSeqLP.lp[8].s = cmd_stat & 0x0400 ? IPS_OK : IPS_IDLE; */
/*       SSD_MotorSeqLP.lp[9].s = cmd_stat & 0x0800 ? IPS_OK : IPS_IDLE; */
      seq_state.ssd_motor_seq= SSD650V_OK ;
      seq_state.ready       = cmd_stat & 0x0001 ? SSD650V_OK : SSD650V_IDLE;
      seq_state.on          = cmd_stat & 0x0002 ? SSD650V_OK : SSD650V_IDLE;
      seq_state.running     = cmd_stat & 0x0004 ? SSD650V_OK : SSD650V_IDLE;
      seq_state.tripped     = cmd_stat & 0x0008 ? SSD650V_OK : SSD650V_IDLE;
      seq_state.coast       = cmd_stat & 0x0010 ? SSD650V_IDLE : SSD650V_OK;
      seq_state.fast_stop   = cmd_stat & 0x0020 ? SSD650V_IDLE : SSD650V_OK;
      seq_state.on_disable  = cmd_stat & 0x0040 ? SSD650V_OK : SSD650V_IDLE;
      seq_state.remote      = cmd_stat & 0x0200 ? SSD650V_OK : SSD650V_IDLE;
      seq_state.sp_reached  = cmd_stat & 0x0400 ? SSD650V_OK : SSD650V_IDLE;
      seq_state.intern_limit= cmd_stat & 0x0800 ? SSD650V_OK : SSD650V_IDLE;

      inverter_seq_status = cmd_stat;
    }
  } else {
    inverter_seq_status = -1;
/*     SSD_device_infoTP.s = IPS_ALERT; */
/*     SSD_MotorSeqLP.s = IPS_ALERT; */
/*     for (i = 0; i < 10; i++) { */
/*       SSD_MotorSeqLP.lp[i].s = IPS_BUSY; */
/*     } */
    seq_state.ssd_motor_seq= SSD650V_ALERT ;
    seq_state.ready       = SSD650V_BUSY ;
    seq_state.on          = SSD650V_BUSY ;
    seq_state.running     = SSD650V_BUSY ;
    seq_state.tripped     = SSD650V_BUSY ;
    seq_state.coast       = SSD650V_BUSY ;
    seq_state.fast_stop   = SSD650V_BUSY ;
    seq_state.on_disable  = SSD650V_BUSY ;
    seq_state.remote      = SSD650V_BUSY ;
    seq_state.sp_reached  = SSD650V_BUSY ;
    seq_state.intern_limit= SSD650V_BUSY ;
  }
  return cmd_stat;
}

/******************************************************************************
 * motor_faststop_switch_state()
 * Switches the motor from running (or ready to run) to fast stopping and back
 * to stopped.
 *****************************************************************************/
void
motor_faststop_switch_state()
{
  char* msg;
  char cmd_word[6];
  int mot_stat;
  int res;

  mot_stat = qry_mot_cmd();
  if (mot_stat < 0) {
    msg = "failure getting motor commanding status.";
    indi_log(ILOG_ERR, msg); 

  } else {
    // wildi ToDo:   if( MotorFastStopSP.sp[0].s == ISS_ON) {
    if (fast_stop_state.set == SSD650V_ON) {
      mot_stat &= 0xfffa;
      sprintf(cmd_word, ">%.4X", mot_stat);
      res = SSD_set_tag(ser_dev, 271, cmd_word);
      msg = "Motor is fast stoping";

      //    } wildi ToDo: else if (MotorFastStopSP.sp[1].s == ISS_ON) {
    } else if (fast_stop_state.reset == SSD650V_ON) {
      mot_stat |= 0x0004;
      sprintf(cmd_word, ">%.4X", mot_stat);
      res = SSD_set_tag(ser_dev, 271, cmd_word);
      msg = "Motor is not fast stoping";

    } else {
      msg = "??? none of the 1-of-many switches is on!";
      indi_log(ILOG_WARNING, msg);
    }
  }
  qry_mot_cmd();
  qry_mot_status();
}

/******************************************************************************
 * motor_coast_switch_state()
 * Switches the motor from running (or ready to run) to coasting and back to
 * stopped.
 *****************************************************************************/
// wildi ToDo: not used here and not planned within RTS2
void
motor_coast_switch_state()
{
  char* msg;
  char cmd_word[6];
  int mot_stat;
  int res;

  mot_stat = qry_mot_cmd();
  if (mot_stat < 0) {
/*     MotorCoastSP.s = IPS_ALERT; */
    msg = "failure getting motor commanding status.";
    indi_log(ILOG_ERR, msg);

  } else {
    // wildi ToDo if (MotorCoastSP.sp[0].s == ISS_ON) {
    if (coast_state.set == SSD650V_ON) {
      mot_stat &= 0xfffc;
      sprintf(cmd_word, ">%.4X", mot_stat);
      res = SSD_set_tag(ser_dev, 271, cmd_word);
      msg = "Motor is coasting";
/*       MotorCoastSP.sp[0].s = ISS_ON; */
/*       MotorCoastSP.sp[1].s = ISS_OFF; */
      coast_state.set  = SSD650V_ON ;
      coast_state.reset= SSD650V_OFF ;

/*       MotorStartSP.sp[0].s = ISS_OFF; */
/*       MotorStartSP.sp[1].s = ISS_ON; */
      motor_state.start= SSD650V_OFF ;
      motor_state.stop = SSD650V_ON ;
  
      // wildi ToDo } else if (MotorCoastSP.sp[1].s == ISS_ON) {
    } else if (coast_state.reset == SSD650V_ON) {
      mot_stat |= 0x0002;
      sprintf(cmd_word, ">%.4X", mot_stat);
      res = SSD_set_tag(ser_dev, 271, cmd_word);
      msg = "Motor is not coasting";
/*       MotorCoastSP.sp[0].s = ISS_OFF; */
/*       MotorCoastSP.sp[1].s = ISS_ON; */
      coast_state.set  = SSD650V_OFF ;
      coast_state.reset= SSD650V_ON ;

    } else {
/*       MotorCoastSP.s = IPS_ALERT; */
      msg = "??? none of the 1-of-many switches is on!";
      indi_log(ILOG_WARNING, msg);
    }
  }

/*   IDSetSwitch(&MotorCoastSP, msg); */
  qry_mot_cmd();
  qry_mot_status();
}

/******************************************************************************
 * motor_run_switch_state()
 * Switches the motor from stopped to running or  to stopped.
 *****************************************************************************/
int motor_run_switch_state()
{
  int res;
  char * msg;

  float setpoint = SSD_qry_setpoint(ser_dev);
  if (!isnan(setpoint)) {
    // OK
  } else {
/*     motorOperationNP.s = IPS_ALERT; */
    indi_log(ILOG_WARNING, "Failure querying setpoint - aborting motor command");
    return SSD650V_GETTING_SET_POINT_FAILED;
  }

// wildi ToDo  if (MotorStartSP.sp[0].s == ISS_ON) {
  if (motor_state.start== SSD650V_ON) {
    msg = "Motor is started";
    fprintf( stderr, "%s\n", msg) ;
    res = set_setpoint(setpoint);
    if (res != BISYNC_OK) {
      indi_log(ILOG_WARNING, "Failure setting setpoint (%d) - motor not started", res);
/*       MotorStartSP.sp[0].s = ISS_OFF; */
/*       MotorStartSP.sp[1].s = ISS_ON; */
      motor_state.start= SSD650V_OFF ;
      motor_state.stop = SSD650V_ON ;

      return SSD650V_SETTING_SET_POINT_FAILED;
    }
    res = SSD_set_tag(ser_dev, 271, ">047F");
/*     MotorCoastSP.sp[0].s = ISS_OFF; */
/*     MotorCoastSP.sp[1].s = ISS_ON; */
    coast_state.set  = SSD650V_OFF ;
    coast_state.reset= SSD650V_ON ;

    qry_mot_cmd();
    qry_mot_status();

    return SSD650V_RUNNING ;

// wildi ToDo  } else if (MotorStartSP.sp[1].s == ISS_ON) {
  } else if (motor_state.stop  == SSD650V_ON) {
    msg = "Motor is stopped";
    fprintf( stderr, "%s\n", msg) ;
    res = SSD_set_tag(ser_dev, 271, ">047E");
/*     MotorCoastSP.sp[0].s = ISS_OFF; */
/*     MotorCoastSP.sp[1].s = ISS_ON; */
    coast_state.set  = SSD650V_OFF ;
    coast_state.reset= SSD650V_ON ;

    qry_mot_cmd();
    qry_mot_status();
    return SSD650V_STOPPED ;
  }
  else
    {
    msg = "------------------ No good value";
    fprintf( stderr, "%s\n", msg) ;
    
    }
  return SSD650V_CONNECTION_NOK ;
}

/******************************************************************************
 * difftimeval()
 * Gets difference of two timeval in milli seconds.
 *****************************************************************************/
time_t
difftimeval(struct timeval *t1, struct timeval *t0)
{
  time_t msec;
  msec = (t1->tv_sec - t0->tv_sec) * 1000;
  msec += (t1->tv_usec - t0->tv_usec) / 1000;
  return msec;
}

/******************************************************************************
 * query_all_ssd650_status()
 *****************************************************************************/
void
query_all_ssd650_status(void * user_p)
{
  int old_debug = debug;
  struct timeval time_enq;

  debug -= 1;
  qry_mot_cmd();
  qry_mot_status();
  debug = old_debug;

  /* send a heart beat */
  gettimeofday(&time_enq, NULL);
  if (difftimeval(&time_enq, &time_last_stat) > 500) {
    time_last_stat = time_enq;
    // wildi ToDo if (SSD_MotorSeqLP.s == IPS_IDLE) {
    if (seq_state.ssd_motor_seq == SSD650V_IDLE) {
      seq_state.ssd_motor_seq= SSD650V_OK; 
    } else {
      seq_state.ssd_motor_seq= SSD650V_IDLE; 
    }
  }
}
/******************************************************************************
 * connectDevice()
 *****************************************************************************/
int connectDevice( int power_state)
{
  int res = 0;
  int mot_stat;
  char device_ssd650v[]="/dev/ssd650v" ;

  switch (power_state) {
    case SSD650V_CONNECT:
      ser_dev = ssd_start_comm(device_ssd650v);
      if (ser_dev >= 0) {
	/*connected: query identity from SSD650V */
        char * id_str = SSD_qry_identity(ser_dev);
        if (id_str) {
/*           IUSaveText(&SSD_device_infoTP.tp[0], id_str); */
          free(id_str);
        } else {
	  return SSD650V_CONNECTION_FAILED ;
        }
        /* query major state from SSD650V indicating powering up and
         * initialisation */
        id_str = SSD_qry_major_state(ser_dev);
        if (id_str) {
/*           IUSaveText(&SSD_device_infoTP.tp[1], id_str); */
        } else {
	  return SSD650V_MAJOR_STATE_FAILED ;
        }
        /* query the motor sequencing status from SSD650V */
        qry_mot_status();
        /* query last communication error condition from SSD650V
         * includes invalid Mnemonic, BCC error, read from write-only, write to
         * read-only, invalid data, out of range data */
        int last_error = SSD_qry_last_error(ser_dev);
        if (last_error >= 0) {
          asprintf(&id_str, "0x%04X", last_error);
/*           IUSaveText(&SSD_device_infoTP.tp[4], id_str); */
          free(id_str);
        } else {
	  return SSD650V_LAST_ERROR_FAILED ;
        }
        /* get current setpoint, accel and decel times */
        float setpoint = SSD_qry_real(ser_dev, 247);

        if (!isnan(setpoint)) {
/*           IDSetNumber(&motorOperationNP, "setpoint currently is %3.1f", abs_setpoint); */
        } else {
          indi_log(ILOG_WARNING, "Failure querying setpoint");
	  return SSD650V_GETTING_SET_POINT_FAILED ;
        }

        float accel_tm = SSD_qry_real(ser_dev, 258);
        if (!isnan(accel_tm)) {
/*           IDSetNumber(&motorOperationNP, "accel time currently is %3.1f", accel_tm); */
	  fprintf( stderr, "connectDevice: accel time currently is %3.1f-----------------------------\n", accel_tm);
        } else {
          indi_log(ILOG_WARNING, "Failure querying accel_tm");
	  fprintf( stderr, "Failure querying accel_tm-----------------------------\n");
	  return SSD650V_GETTING_ACCELERATION_TIME_FAILED ;
        }

        float decel_tm = SSD_qry_real(ser_dev, 259);
        if (!isnan(decel_tm)) {
	  fprintf( stderr, "connectDevice: decel time currently is %3.1f-----------------------------\n", decel_tm);
        } else {
          indi_log(ILOG_WARNING, "Failure querying decel_tm");
	  fprintf( stderr, "Failure querying decel_tm-----------------------------\n");
	  fprintf( stderr, "-----------------------------\n");
	  return SSD650V_GETTING_DECELERATION_TIME_FAILED ;
        }
        /* query the motor control command status from SSD650V */
        mot_stat = qry_mot_cmd();
        if (mot_stat >= 0) {
          if ((mot_stat & 0x0001) != 0) {
/*             MotorStartSP.sp[1].s = ISS_OFF; */
	    motor_state.stop= SSD650V_OFF ;
            if (setpoint >= 0) {
/*               MotorStartSP.sp[0].s = ISS_ON; */
/*               MotorStartSP.sp[2].s = ISS_OFF; */
	      motor_state.start= SSD650V_ON ;

            } else {
/*               MotorStartSP.sp[0].s = ISS_OFF; */
/*               MotorStartSP.sp[2].s = ISS_ON; */
	      motor_state.start= SSD650V_OFF ;
            }
          } else {
/*             MotorStartSP.sp[0].s = ISS_OFF; */
/*             MotorStartSP.sp[1].s = ISS_ON; */
	      motor_state.start= SSD650V_OFF ;
	      motor_state.stop = SSD650V_ON ;
          }
          if ((mot_stat & 0x0002) != 0) {
/*             MotorCoastSP.sp[0].s = ISS_OFF; */
/*             MotorCoastSP.sp[1].s = ISS_ON; */
	    coast_state.set  = SSD650V_OFF ;
	    coast_state.reset= SSD650V_ON ;
	    
          } else {
/*             MotorCoastSP.sp[0].s = ISS_ON; */
/*             MotorCoastSP.sp[1].s = ISS_OFF; */
	    coast_state.set  = SSD650V_ON ;
	    coast_state.reset= SSD650V_OFF ;
          }

          if ((mot_stat & 0x0004) != 0) {
/*             MotorFastStopSP.sp[0].s = ISS_OFF; */
/*             MotorFastStopSP.sp[1].s = ISS_ON; */
	    fast_stop_state.set  = SSD650V_OFF ;
	    fast_stop_state.reset= SSD650V_ON ;
          } else {
/*             MotorFastStopSP.sp[0].s = ISS_ON; */
/*             MotorFastStopSP.sp[1].s = ISS_OFF; */
	    fast_stop_state.set  = SSD650V_ON ;
	    fast_stop_state.reset= SSD650V_OFF ;
          }
        } else {
	  return SSD650V_GETTING_MOTOR_COMMAND_FAILED ;
        }
	// wildi ToDo gettimeofday(&time_last_stat, NULL);
        // wildi ToDo workproc_id = addWorkProc(&query_all_ssd650_status, NULL);
      } else {
	return SSD650V_CONNECTION_NOK ;
      }
      // set the initial values
      fast_stop_state.set = SSD650V_OFF ;
      fast_stop_state.reset = SSD650V_ON ;
      motor_faststop_switch_state() ;

      coast_state.set  = SSD650V_OFF ;
      coast_state.reset= SSD650V_ON ;

      motor_coast_switch_state() ;
      break;

    case SSD650V_DISCONNECT:
      // wildi ToDo rmWorkProc(workproc_id);

      res = ssd_stop_comm();
      if (!res) {
	// wildi ToDo  IDSetSwitch(&PowerSP, "disconnected from %s.", SerDeviceTP.tp[0].text);
      } else {
        // wildi ToDo IDSetSwitch(&PowerSP, "disconnect from %s failed.", SerDeviceTP.tp[0].text);
      }
      break;

    default: ;
  }
  return SSD650V_GENERAL_FAILURE;
}
/******************************************************************************
 * ssd_start_comm()
 *****************************************************************************/
int
ssd_start_comm(char * ser_dev_name)
{
  int sd;  /* serial device file descriptor */
  int bitrate  = DEFAULT_BITRATE;
  int databits = DEFAULT_DATABITS;
  int parity   = DEFAULT_PARITY;
  int stopbits = DEFAULT_STOPBITS;

  // open serial ports
  if ((sd = serial_init(ser_dev_name, bitrate, databits,
                        parity, stopbits)) < 0)
  {
    indi_log(ILOG_ERR, "open of %s failed.", ser_dev_name);
    return -1;
  }
  ser_open = 1;
  bisync_init();

  return sd;
}

/******************************************************************************
 * ssd_stop_comm()
 *****************************************************************************/
int
ssd_stop_comm()
{
  int res = 0;
  if (ser_open) {
    res = serial_shutdown(ser_dev);
    indi_debug_log(1, "restored terminal settings on serial port.");
  }
  return res;
}

/******************************************************************************
 * SSD_tag2memonic(...)
 * Translates an SSD650V parameter tag number to the mnemonic id which
 * can be used to specifiy a parameter to set or enquire via BISYNC protocol.
 * mnemonic must point to a char array of at least 3 chars size.
 *****************************************************************************/
void
SSD_tag2mnemonic(int tag, char *mnemonic)
{
  unsigned char m;
  unsigned char n;

  if (tag < 1296) {
    m = tag / 36;
    n = tag % 36;

    if (m > 9) {
      mnemonic[0] = 'a' + m - 10;
    } else {
      mnemonic[0] = '0' + m;
    }
    if (n > 9) {
      mnemonic[1] = 'a' + n - 10;
    } else {
      mnemonic[1] = '0' + n;
    }

  } else {
    m = (tag - 1296) / 126;
    n = (tag - 1296) % 26;
    mnemonic[0] = 'a' + n;
    mnemonic[1] = 'A' + m;
  }
  mnemonic[2] = '\0';
}


/******************************************************************************
 * SSD_chkresp_bool(...)
 * Checks the given response frame for a valid boolean value.
 * The response frame buffer is freed before return.
 * Return:
 *   the data from the reply as a BOOLEAN (0=false, 1=true) or -1 on error.
 *****************************************************************************/
int
SSD_chkresp_bool(int sd, char *response_frame)
{
  BOOLEAN b = FALSE;

  free(response_frame);
  return b;
}

/******************************************************************************
 * SSD_chkresp_tagnbr(...)
 * Checks the given response frame for a valid boolean value.
 * The response frame buffer is freed before return.
 * Return:
 *   the data from the reply as a BOOLEAN (0=false, 1=true) or -1 on error.
 *****************************************************************************/
int
SSD_chkresp_tagnbr(int sd, char *response_frame)
{
  int i = 0;

  free(response_frame);
  return i;
}

/******************************************************************************
 * SSD_chkresp_real(...)
 * Checks the given response frame for a valid real value.
 * The response frame buffer is freed before return.
 * Return:
 *   the data from the reply as a float or NaN on error.
 *****************************************************************************/
float
SSD_chkresp_real(int sd, char *response_frame)
{
  float st_real;

  if (((response_frame[3] < '0') || (response_frame[3] > '9'))
      && (response_frame[3] != '.') && (response_frame[3] != '-'))
  {
    indi_log(ILOG_ERR, "SSD_qry_real(): reply has wrong data type: %s.",
                       &response_frame[3]);
    free(response_frame);
    return sqrt(-1);  /* return NaN */
  }

  int cnt = sscanf(&response_frame[3], "%f", &st_real);
  if (cnt != 1) {
    indi_log(ILOG_ERR, "Expected a real type reply but was: %s.",
                       &response_frame[3]);
    free(response_frame);
    return sqrt(-1);  /* return NaN */
  }

  free(response_frame);
  return st_real;
}

/******************************************************************************
 * SSD_qry_real(...)
 * Queries the SSD650V for the value of the given parameter tag. The queried
 * tag must be of real data type.
 * Return:
 *   the data from the reply as a float or NaN on error.
 *****************************************************************************/
float
SSD_qry_real(int sd, int tag)
{
  int err;
  char *response_frame;
  char mn[3];

  SSD_tag2mnemonic(tag, mn);
  err = bisync_enquiry(sd, 0, 0, mn, &response_frame, ENQUIRY_SINGLE);
  if (err) {
    indi_log(ILOG_ERR, "SSD_qry_real() for mnemo \"%s\" failed (%d: %s).",
                       mn, err, bisync_errstr(err));
    return sqrt(-1);  /* return NaN */
  }

  return SSD_chkresp_real(sd, response_frame);
}

/******************************************************************************
 * SSD_chkresp_integer(...)
 * Checks the given response frame for a valid integer value.
 * The response frame buffer is freed before return.
 * Return:
 *   the data from the reply as an positive integer or -1 on error.
 *****************************************************************************/
int
SSD_chkresp_integer(int sd, char *response_frame)
{
  unsigned int st_int;

  if ((response_frame[3] < '0') || (response_frame[3] > '9')){
    indi_log(ILOG_ERR, "SSD_qry_integer(): reply has wrong data type: %s.",
                       &response_frame[3]);
    free(response_frame);
    return -1;
  }

  int cnt = sscanf(&response_frame[3], "%d.", &st_int);
  if (cnt != 1) {
    indi_log(ILOG_ERR, "Expected an integer reply but was: %s.",
                       &response_frame[3]);
    free(response_frame);
    return -1;
  }

  free(response_frame);
  return st_int;
}

/******************************************************************************
 * SSD_qry_integer(...)
 * Queries the SSD650V for the value of the given parameter tag. The queried
 * tag must be of unsigned integer data type.
 * Return:
 *   the data from the reply as an positive integer or -1 on error.
 *****************************************************************************/
int
SSD_qry_integer(int sd, int tag)
{
  int err;
  char *response_frame;
  char mn[3];

  SSD_tag2mnemonic(tag, mn);
  err = bisync_enquiry(sd, 0, 0, mn, &response_frame, ENQUIRY_SINGLE);
  if (err) {
    fprintf(stderr, "SSD_qry_integer() for tag %d failed (%d: %s)\n",
                    tag, err, bisync_errstr(err));
    return -1;
  }

  return SSD_chkresp_integer(sd, response_frame);
}

/******************************************************************************
 * SSD_chkresp_hexword(...)
 * Checks the given response frame for a valid hex word value.
 * The response frame buffer is freed before return.
 * Return:
 *   the data from the reply as a positive integer or -1 on error.
 *****************************************************************************/
int
SSD_chkresp_hexword(int sd, char *response_frame)
{
  unsigned int st_word;

  if (response_frame[3] != '>') {
    indi_log(ILOG_ERR, "SSD__hexword(): reply has wrong data type: %s.",
                       &response_frame[3]);
    free(response_frame);
    return -1;
  }

  int cnt = strlen(&response_frame[4]);
  if (cnt != 4) {
    indi_log(ILOG_ERR, "SSD__hexword(): hex word reply has wrong "
                       "data length: %s.", &response_frame[4]);
    free(response_frame);
    return -1;
  }

  cnt = sscanf(&response_frame[4], "%X", &st_word);
  if (cnt != 1) {
    indi_log(ILOG_ERR, "Expected a hex word but reply was: %s.",
                       &response_frame[3]);
    free(response_frame);
    return -1;
  }

  free(response_frame);
  return st_word;
}

/******************************************************************************
 * SSD_qry_hexword(...)
 * Queries the SSD650V for the value of the given parameter tag. The queried
 * tag must be of hex word data type.
 * Return:
 *   the data from the reply as a positive integer or -1 on error.
 *****************************************************************************/
int
SSD_qry_hexword(int sd, int tag)
{
  int err;
  char *response_frame;
  char mn[3];
  
  SSD_tag2mnemonic(tag, mn);
  err = bisync_enquiry(sd, 0, 0, mn, &response_frame, ENQUIRY_SINGLE);
  if (err) {
    indi_log(ILOG_ERR, "SSD_qry_hexword() for tag %d failed (%d: %s)",
                    tag, err, bisync_errstr(err));
    return -1;
  }

  return SSD_chkresp_hexword(sd, response_frame);
}

/******************************************************************************
 * SSD_qry_last_error(..)
 * Query the SSD650V for it's last error status.
 * Return:
 *****************************************************************************/
int 
SSD_qry_last_error(int sd)
{
  char *mn;
  int err;
  char *response_frame;
  unsigned int err_word;

  /* query instrument identity */
  mn = "EE";
  err = bisync_enquiry(sd, 0, 0, mn, &response_frame, ENQUIRY_SINGLE);
  if (err) {
    fprintf(stderr, "Query of last error state failed (%d: %s)\n",
                    err, bisync_errstr(err));
    return -1;
  }

  if (response_frame[3] != '>') {
    fprintf(stderr, "SSD_qry_last_error(): reply has wrong data type: %s\n",
                    &response_frame[3]);
    free(response_frame);
    return -1;
  }

  int cnt = sscanf(&response_frame[4], "%X", &err_word);
  if (cnt != 1) {
    fprintf(stderr, "Expected a hex word reply but was: %s\n",
                    &response_frame[3]);
    free(response_frame);
    return -1;
  }
  free(response_frame);

  return err_word;
}

/******************************************************************************
 * SSD_qry_identity(...)
 * Query the SSD650V for it's instrument identity and it's software version.
 * Return:
 *   A newly allocated string describing the connected instrument
 *   or NULL if something is wrong.
 *   (don't forget to free() the returned string buffer, when not needed
 *    anymore.)
 *****************************************************************************/
char *
SSD_qry_identity(int sd)
{
  char *mn;
  int err;
  char *response_frame;
  char *ii_str;
  
  /* query instrument identity */
  mn = "II";
  err = bisync_enquiry(sd, 0, 0, mn, &response_frame, ENQUIRY_SINGLE);
  if (err) {
    fprintf(stderr, "Query of instrument identity failed (%d: %s)\n",
                    err, bisync_errstr(err));
    return NULL;
  }

  if (response_frame[3] != '>') {
    fprintf(stderr, "SSD_qry_identity(): reply has wrong data type: %s\n",
                    &response_frame[3]);
    free(response_frame);
    return NULL;
  }

  if (strcmp("1650", &response_frame[4])) {
    fprintf(stderr, "Not the expected instrument model, reply was: \"%s\"\n",
                    &response_frame[1]);
    free(response_frame);
    return NULL;
  }
  if (debug > 1) {
    fprintf(stderr, "Got instrument identity: \"%s\"\n", &response_frame[4]);
  }
  free(response_frame);
 
  /* query instrument main software version */
  mn = "V0";
  err = bisync_enquiry(sd, 0, 0, mn, &response_frame, ENQUIRY_SINGLE);
  if (err) {
    fprintf(stderr, "Query of instrument main software version failed (%d: %s)\n",
                    err, bisync_errstr(err));
    return NULL;
  }

  if (response_frame[3] != '>') {
    fprintf(stderr, "SSD_qry_identity(): reply has wrong data type: %s\n",
                    &response_frame[3]);
    free(response_frame);
    return NULL;
  }
  if (debug > 1) {
    fprintf(stderr, "Got instrument software version: \"%s\"\n", &response_frame[4]);
  }

  ii_str = concat("SSD650V, ver. ", &response_frame[4], NULL);
  free(response_frame);
  return ii_str;
}

/******************************************************************************
 * SSD_qry_major_state(...)
 * Query SSD650V's major state.
 * Return:
 *   A string describing SSD650V's major state
 *   or NULL if something is wrong.
 *****************************************************************************/
char *
SSD_qry_major_state(int sd)
{
  char *mn;
  int err;
  char *response_frame;
  unsigned int st_word;
  char *ii_str;

  mn = "!2";
  err = bisync_enquiry(sd, 0, 0, mn, &response_frame, ENQUIRY_SINGLE);
  if (err) {
    fprintf(stderr, "Query of SSD650V's major state failed (%d: %s)\n",
                    err, bisync_errstr(err));
    return NULL;
  }

  if (response_frame[3] != '>') {
    fprintf(stderr, "SSD_qry_major_state(): reply has wrong data type: %s\n",
                    &response_frame[3]);
    free(response_frame);
    return NULL;
  }

  int cnt = sscanf(&response_frame[4], "%X", &st_word);
  if (cnt != 1) {
    fprintf(stderr, "SSD_qry_major_state(): Expected a hex word reply "
                    "but was: %s\n", &response_frame[3]);
    free(response_frame);
    return NULL;
  }
  free(response_frame);

  switch (st_word) {
    case 0:
      ii_str = "Initialising";
      break;
    case 1:
      ii_str = "Corrupted product code and configuration";
      break;
    case 2:
      ii_str = "Corrupted configuration";
      break;
    case 3:
      ii_str = "Restoring configuration";
      break;
    case 4:
      ii_str = "Re-configuring mode";
      break;
    case 5:
      ii_str = "Normal operation mode";
      break;
    default:
      ii_str = "unknown (illegal) state";
  }
  return ii_str;
}

/******************************************************************************
 * SSD_qry_comms_status(...)
 * Queries the comm status prameter from the SSD650V via the open serial port
 * given in the serial port device descriptor `ser_dev' using bisync protocol.
 * Return:
 *   positive integer value of the replied 16-bit hex value or negative value
 *   if an error occured.
 *****************************************************************************/
int
SSD_qry_comms_status(int sd)
{
  return SSD_qry_hexword(sd, 272);
}


/******************************************************************************
 * SSD_qry_setpoint(...)
 * Queries the motor speed setpoint from the SSD650V via the open serial port.
 * Return:
 *   value of the current setpoint
 *   or NaN if an error occured.
 *****************************************************************************/
float
SSD_qry_setpoint(int sd)
{
  return SSD_qry_real(sd, 247);
}

/******************************************************************************
 * SSD_set_tag(...)
 *****************************************************************************/
int
SSD_set_tag(int sd, int tag, char * data)
{
  char mn[3];
  int err;

  if (strlen(data) > 7) {
    fprintf(stderr, "SSD_set_tag(): data string too long.\n");
    return BISYNC_ERR;
  }

  SSD_tag2mnemonic(tag, mn);
  //indi_debug_log(1, "going to set parameter tag %s to %s.", mn, data);
  err = bisync_setparam(sd, 0, 0, mn, data, SETPARAM_SINGLE);
  if (err) {
    fprintf(stderr, "set param tag %d to \"%s\" failed (%d: %s)\n",
                    tag, data, err, bisync_errstr(err));
    return err;
  }

  return BISYNC_OK;
}

/******************************************************************************
 * SSD_set_mnemo(...)
 *****************************************************************************/
int
SSD_set_mnemo(int sd, char * mn, char * data)
{
  int err;

  if (strlen(data) > 7) {
    fprintf(stderr, "SSD_set_mnemo(): data string too long.\n");
    return BISYNC_ERR;
  }

  //indi_debug_log(1, "going to set parameter tag %s to %s.", mn, data);
  err = bisync_setparam(sd, 0, 0, mn, data, SETPARAM_SINGLE);
  if (err) {
    fprintf(stderr, "set param mnemo \"%s\" to \"%s\" failed (%d: %s)\n",
                    mn, data, err, bisync_errstr(err));
    return err;
  }

  return BISYNC_OK;
}

/******************************************************************************
 * SSD_query(...)
 *****************************************************************************/
int
SSD_query(int sd, tagdata_t *param)
{
  int err = BISYNC_OK;
  char *response_frame;
  char mn[3];

  if (param->param_perm == PARAM_RO) {
    err = BISYNC_ERR;

  } else {
    if (param->cmd_format == CMDF_TAGNBR) {
      SSD_tag2mnemonic(param->command.tag, mn);

    } else if (param->cmd_format == CMDF_MNEMO) {
      memcpy(mn, param->command.mnemo, 3);
    }

    err = bisync_enquiry(sd, 0, 0, mn, &response_frame, ENQUIRY_SINGLE);
    if (err) {
      indi_log(ILOG_ERR, "SSD_query() for mnemo \"%s\" failed (%d: %s)\n",
                          mn, err, bisync_errstr(err));
    }
  }

  switch (param->data_type) {
    case TAGT_REAL:
      param->data.f = SSD_chkresp_real(sd, response_frame);
      break;
    case TAGT_WORD:
      param->data.i = SSD_chkresp_hexword(sd, response_frame);
      break;
    case TAGT_INT:
      break;
    case TAGT_BOOL:
      param->data.b = SSD_chkresp_bool(sd, response_frame);
      break;
    case TAGT_NBR:
      param->data.tag_nbr = SSD_chkresp_tagnbr(sd, response_frame);
      break;
    case TAGT_ENUM:
      break;
  } // switch param->data_type)

  return err;
}

/******************************************************************************
 * SSD_run_cmd_sequence(...)
 *****************************************************************************/
int
SSD_run_cmd_sequence(int sd, tagdata_t * cmd_seq, int cmd_list_sz)
{
  int res = 0;
  int i;
  for (i = 0; i < cmd_list_sz; i++) {
  }
  return res;
}

/******************************************************************************
 * SSD_comm_queue(...)
 * Runs as a 
 *****************************************************************************/
void
SSD_comm_queue()
{
}

/******************************************************************************
 * SSD_setup(...)
 *****************************************************************************/
void
SSD_setup()
{
  /*
  The important commands for using an SSD650V frequency inverter
  (see svn-vermes/experiment/perl/650V_init.sh):
  
  type value   data     group comment

  mnem    !1   >7777    0     # Reset everything
  delay    1   delay    0     #
  mnem    !1   >0101    0     # Restore configuration from non-volatile memory
  delay    3   delay    0     #

  mnem    !1   >5555    0     # Enter configuration mode
  tag    300   >01      1     # Remote comms select
  mnem    !1   >4444    0     # Exit configuration mode

  tag    265   1.       1     # Ref Modes: local only
  tag    247   90.      1     # Local Setpoint
  tag    251   20.      1     # Local Min Speed
  tag    271   >047E    1     # Communications Command: enable drive, reset fault
  tag    299   1.       1     # Power Up Mode: remote

  tag   1159   50.0     2     # Motor Data, Base Frequency
  tag   1160   230.0    2     # Motor Data, Motor Voltage
  tag     64   0.75     2     # Motor Data, Motor Current
  tag     83   1350.0   2     # Motor Data, Nameplate rpm
  tag     84   4.       2     # Motor Data, Motor Poles
  tag   1158   .07      2     # Motor Data, Motor Power
  tag    124   0.       2     # Motor Data, Motor Connection: Delta
  tag    258   5.       3     # Reference Ramp Accel Time
  tag    259   2.       3     # Reference Ramp Decel Time

  mnem    !3   >0001    6     # Save configuration to non-volatile memory

  tag    272   query    4     # Query comms sequencing state

  tag    271   >047E    4     # Communications Sequencing Command: initial comms sequencing setup
  tag    271   >047F    4     # Communications Sequencing Command: run forward
  #
  tag    271   >047E    5     # Communications Sequencing Command: stop
  #
  mnem    EE   >0000   -1     # Reset last error code (when enquiry: get last error code)
  mnem    II   -       -1     # (read only) query instrument identity
  mnem    V0   -       -1     # (read only) query instrument main software version
  mnem    V1   -       -1     # (read only) query instrument keypad software version

  mnem    !4   -       -1     # (read only) query state of non-volatile saving operation:
                              #   >0000: idle; >0001: saving; >0002: failed

  */
}

