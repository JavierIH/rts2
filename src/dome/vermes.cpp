/* 
 * Obs. Vermes cupola driver.
 * Copyright (C) 2010 Markus Wildi <markus.wildi@one-arcsec.org>
 * based on Petr Kubanek's dummy_cup.cpp
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

#include "cupola.h"
#include "../utils/rts2config.h" 
#include "vermes.h" 

#ifdef __cplusplus
extern "C"
{
#endif
// wildi ToDo: go to dome-target-az.h
double dome_target_az( struct ln_equ_posn tel_eq, int angle, struct ln_lnlat_posn *obs) ;
#ifdef __cplusplus
}
#endif

#include "barcodereader_vermes.h"
#define MOTOR_RUNNING      0
#define MOTOR_NOT_RUNNING  1
#define MOTOR_UNDEFINED    2

int motor_on_off_state ;
int barcodereader_state ;
double barcodereader_az ;
double barcodereader_dome_azimut_offset= -253.6 ; // wildi ToDo: make an option
double target_az ;
struct ln_lnlat_posn *obs ;
struct ln_equ_posn tel_eq ;



#ifdef __cplusplus
extern "C"
{
#endif
int set_setpoint( float setpoint) ;
float get_setpoint() ;
int motor_on() ;
int motor_off() ;
int connectDevice( int power_state) ;
#ifdef __cplusplus
}
#endif

void *compare_azimuth( void *value) ;


using namespace rts2dome;

namespace rts2dome
{
/**
 * Obs. Vermes cupola driver.
 *
 * @author Markus Wildi <markus.wildi@one-arcsec.org>
 */
  class Vermes:public Cupola
  {
  private:
    Rts2Config *config ;
    Rts2ValueInteger *barcode_reader_state ;
    Rts2ValueDouble  *azimut_difference ;
    Rts2ValueString  *ssd650v_state ;
    Rts2ValueBool    *ssd650v_on_off ;
    Rts2ValueDouble  *ssd650v_setpoint ;

    void parkCupola ();
  protected:
    virtual int moveStart () ;
    virtual int moveEnd () ;
    virtual long isMoving () ;
    // there is no dome door to open 
    virtual int startOpen (){return 0;}
    virtual long isOpened (){return -2;}
    virtual int endOpen (){return 0;}
    virtual int startClose (){return 0;}
    virtual long isClosed (){return -2;}
    virtual int endClose (){return 0;}

  public:
    Vermes (int argc, char **argv) ;
    virtual int initValues () ;
    virtual double getSplitWidth (double alt) ;
    virtual int info () ;
    virtual int idle ();
    virtual void valueChanged (Rts2Value * changed_value) ;
    // park copula
    virtual int standby ();
    virtual int off ();
  };
}

int Vermes::moveEnd ()
{
  //	logStream (MESSAGE_ERROR) << "Vermes::moveEnd set Az "<< hrz.az << sendLog ;
  logStream (MESSAGE_ERROR) << "Vermes::moveEnd did nothing "<< sendLog ;
  return Cupola::moveEnd ();
}
long Vermes::isMoving ()
{
  logStream (MESSAGE_DEBUG) << "Vermes::isMoving"<< sendLog ;



  if ( 1) // if there, return -2
    return -2;
  return USEC_SEC;
}
int Vermes::moveStart ()
{
  tel_eq.ra= getTargetRa() ;
  tel_eq.dec= getTargetDec() ;

  logStream (MESSAGE_ERROR) << "Vermes::moveStart RA " << tel_eq.ra  << " Dec " << tel_eq.dec << sendLog ;

  target_az= dome_target_az( tel_eq, -1,  obs) ; // this value is only informational wildi ToDo: DecAxis!
  logStream (MESSAGE_ERROR) << "Vermes::moveStart dome target az" << target_az << sendLog ;
  setTargetAz(target_az) ;
  return Cupola::moveStart ();
}

double Vermes::getSplitWidth (double alt)
{
  logStream (MESSAGE_ERROR) << "Vermes::getSplitWidth returning 1" << sendLog ;
  return 1;
}

void Vermes::parkCupola ()
{
  logStream (MESSAGE_ERROR) << "Vermes::parkCupola doing nothing" << sendLog ;
}

int Vermes::standby ()
{
  logStream (MESSAGE_ERROR) << "Vermes::standby doing nothing" << sendLog ;
  parkCupola ();
  return Cupola::standby ();
}

int Vermes::off ()
{
//   if(connectDevice(SSD650V_DISCONNECT))
//     {
//       logStream (MESSAGE_ERROR) << "Vermes::off a general failure occured" << sendLog ;
//     }

  logStream (MESSAGE_ERROR) << "Vermes::off NOT disconnecting from frequency inverter" << sendLog ;
  parkCupola ();
  return Cupola::off ();
}

void Vermes::valueChanged (Rts2Value * changed_value)
{
  int res ;
  if (changed_value == ssd650v_on_off)
    {
      if( ssd650v_on_off->getValueBool())
	{
	  logStream (MESSAGE_DEBUG) << "Vermes::valueChanged starting azimuth motor, setpoint: "<< ssd650v_setpoint->getValueDouble() << sendLog ;
	  if(( res=motor_on()) != SSD650V_RUNNING )
	    {
	      logStream (MESSAGE_ERROR) << "Vermes::valueChanged something went wrong with  azimuth motor, error: "<< ssd650v_setpoint->getValueDouble() << sendLog ;
	      motor_on_off_state= MOTOR_UNDEFINED ;
	    }
	  else
	    {
	      fprintf( stderr, "compare_azimuth: 30\n") ;
	      ssd650v_state->setValueString("motor running") ;
	      motor_on_off_state= MOTOR_RUNNING ;
	    }
	}
      else
	{
	  logStream (MESSAGE_DEBUG) << "Vermes::valueChanged stopping azimuth motor, setpoint: "<< ssd650v_setpoint->getValueDouble() << sendLog ;
	  if(( res=motor_off()) !=  SSD650V_STOPPED)
	    {
	      logStream (MESSAGE_ERROR) << "Vermes::valueChanged something went wrong with  azimuth motor, error: "<< ssd650v_setpoint->getValueDouble() << sendLog ;
	      motor_on_off_state= MOTOR_UNDEFINED ;
	    }
	  else
	    {
	      ssd650v_state->setValueString("motor stopped") ;
	      motor_on_off_state= MOTOR_NOT_RUNNING ;
	    }
	}
      return ;
    }
  else if (changed_value == ssd650v_setpoint)
    {
      float setpoint= (float) ssd650v_setpoint->getValueDouble() ;

      if( set_setpoint( setpoint)) 
	{
	  logStream (MESSAGE_ERROR) << "Vermes::valueChanged could not set setpoint "<< setpoint << sendLog ;
	}
      return ; // ask Petr what to do in general if something fails within ::valueChanged
    }
  Cupola::valueChanged (changed_value);
}
int Vermes::idle ()
{
	return Cupola::idle ();
}
int Vermes::info ()
{
  barcode_reader_state->setValueInteger( barcodereader_state) ; 
  setCurrentAz (barcodereader_az);
  setTargetAz(target_az) ;

  azimut_difference->setValueDouble(( barcodereader_az- getTargetAz())) ;
  
  if( motor_on_off_state== MOTOR_RUNNING)
    {
      ssd650v_on_off->setValueBool(true) ;
      ssd650v_state->setValueString("motor running") ;
    }
  else if( motor_on_off_state== MOTOR_NOT_RUNNING)
    {
      ssd650v_on_off->setValueBool(false) ;
      ssd650v_state->setValueString("motor stopped") ;
    }
  else 
    {
      ssd650v_state->setValueString("motor undefined state") ;
    }
  ssd650v_setpoint->setValueDouble( (double) get_setpoint()) ;

  // not Cupola::info() ?!?
  return Cupola::info ();
}
int Vermes::initValues ()
{
  int ret ;
  pthread_t  thread_0;


  config = Rts2Config::instance ();

  ret = config->loadFile ();
  if (ret)
    return -1;

  obs= Cupola::getObserver() ;

  // barcode azimuth reader
  if(!( ret= start_bcr_comm()))
    {
      register_pos_change_callback(position_update_callback);
    }
  else
    {
      logStream (MESSAGE_ERROR) << "Vermes::initValues could connect to barcode devices, exiting "<< sendLog ;
      exit(1) ;
    }
  // ssd650v frequency inverter
  if(connectDevice(SSD650V_CONNECT))
    {
      logStream (MESSAGE_ERROR) << "Vermes::initValues a general failure on SSD650V connection occured" << sendLog ;
    }
  if(( ret=motor_off()) != SSD650V_STOPPED )
    {
      fprintf( stderr, "Vermes::initValues something went wrong with SSD650V (OFF)\n") ;
      motor_on_off_state= MOTOR_UNDEFINED ;
    }
  else
    {
      motor_on_off_state= MOTOR_NOT_RUNNING ;
    }
  // thread to compare (target - current) azimuth and rotate the dome
  int *value ;
  ret = pthread_create( &thread_0, NULL, compare_azimuth, value) ;
  return Cupola::initValues ();
}
Vermes::Vermes (int in_argc, char **in_argv):Cupola (in_argc, in_argv) 
{
  // since this driver is Obs. Vermes specific no options are really required
  createValue (azimut_difference,   "AZdiff",     "target - actual azimuth reading", false, RTS2_DT_DEGREES  );
  createValue (barcode_reader_state,"BCRstate",   "state of the barcodereader value CUP_AZ (0=valid, 1=invalid)", false);
  createValue (ssd650v_state,       "SSDstate",   "status of the ssd650v inverter ", false);
  createValue (ssd650v_on_off,      "SSDswitch",  "(true=running, false=not running)", false, RTS2_VALUE_WRITABLE);
  createValue (ssd650v_setpoint,    "SSDsetpoint","ssd650v setpoint", false, RTS2_VALUE_WRITABLE);

  barcode_reader_state->setValueInteger( -1) ; 
}
int main (int argc, char **argv)
{
	Vermes device (argc, argv);
	return device.run ();
}

//It is not the fastest dome, one revolution in 5 minutes
#define AngularSpeed 2. * M_PI/ 98. 
#define POLLMICROS 1000. * 1000. 
#define DIFFMAX 60 /* Difference where curMaxSetPoint is reached */
#define DIFFMIN  5 /* Difference where curMinSetPoint is reached*/

void *compare_azimuth( void *value)
{
  double ret = -1 ;
  double curAzimutDifference ;
  double curMaxSetPoint= 80 ;
  double curMinSetPoint= 40 ;
  static double lastSetPoint=0, curSetPoint=0 ;
  static double lastSetPointSign=0, curSetPointSign=0 ;
  double tmpSetPoint= 0. ;

/* Driver ac sends positive values for a positive rotation*/
/* Azimut is counted positive from North to east point */
  static double sign_snoop= -1. ; 

  while( 1==1)
    {

      target_az= dome_target_az( tel_eq, -1,  obs) ; // wildi ToDo: DecAxis!
      curAzimutDifference=  barcodereader_az- target_az;

      // fmod is here just in case if there is something out of bounds
      curAzimutDifference= fmod( curAzimutDifference, 360.) ;

      // Shortest path
      if(( curAzimutDifference) >= 180.)
 	{
	  curAzimutDifference -=360. ;
 	}
      else if(( curAzimutDifference) < -180.)
 	{
	  curAzimutDifference += 360. ;
 	}

      if(( motor_on_off_state= MOTOR_RUNNING)&&( ret= fabs( curAzimutDifference/180. * M_PI)) < ( .25 * AngularSpeed * (POLLMICROS/(1000. * 1000.))))
	{
	  fprintf( stderr, "compare_azimuth: absolute difference smaller than %3.1f [deg]\n", ( .25 * AngularSpeed * (POLLMICROS/(1000. * 1000.))) * 180./M_PI) ;
	  curAzimutDifference=  0. ;
 	}
      else if (( ret= fabs( curAzimutDifference/180. * M_PI)) < ( .25 * AngularSpeed * (POLLMICROS/(1000. * 1000.))))
	{
	  fprintf( stderr, "compare_azimuth: NOT RUNNING absolute difference smaller than %3.1f [deg]\n", ( .25 * AngularSpeed * (POLLMICROS/(1000. * 1000.))) * 180./M_PI) ;
	}
      //  Difference at ac,  curMaxSetPoint, curMinSetPoint are always > 0, setpoint is [-100.,100.] 
      tmpSetPoint= (( curMaxSetPoint- curMinSetPoint) / (DIFFMAX- DIFFMIN) * fabs( curAzimutDifference)  + curMinSetPoint ) ;
      curSetPointSign= sign_snoop * curAzimutDifference/fabs(curAzimutDifference) ;

      if( tmpSetPoint > curMaxSetPoint)
	{
	  tmpSetPoint= curMaxSetPoint;
	}
      else if( tmpSetPoint < curMinSetPoint)
	{
	  tmpSetPoint= curMinSetPoint ;
	}
      curSetPoint= curSetPointSign * tmpSetPoint ;
      
      if( curAzimutDifference == 0.)
	{
	  // not stopped
	  if( motor_on_off_state== MOTOR_RUNNING)
	  {
	    fprintf( stderr, "compare_azimuth: detected a zero value %f, stopping", curAzimutDifference);
	    // LDMotorOFF() ;
	    if(( ret=motor_off()) != SSD650V_STOPPED )
	      {
		fprintf( stderr, "compare_azimuth: something went wrong with  azimuth motor (OFF)\n") ;
		motor_on_off_state= MOTOR_UNDEFINED ;
	      }
	    else
	      {
		fprintf( stderr, "compare_azimuth: 20\n") ;
		motor_on_off_state= MOTOR_NOT_RUNNING ;
	      }
	    curSetPoint= 0. ;
	    set_setpoint( curSetPoint) ;
	  }
	  lastSetPoint= 0. ;
	  lastSetPointSign= 0. ;
	}
      else
	{
	  if( lastSetPoint != 0) /* Motor is possibly ON */
	    {
	      if( curSetPointSign ==  lastSetPointSign) /* movement same direction */
		{
		  set_setpoint( curSetPoint) ;

		  // motor is not running
		  if( motor_on_off_state== MOTOR_NOT_RUNNING)
		    {
		      //  LDMotorON() ;
		      if(( ret=motor_on()) != SSD650V_RUNNING )
			{
			  fprintf( stderr, "compare_azimuth: something went wrong with  azimuth motor (ON)\n") ;
			  motor_on_off_state= MOTOR_UNDEFINED ;
			}
		      else
			{
			  fprintf( stderr, "compare_azimuth: 1\n") ;
			  motor_on_off_state= MOTOR_RUNNING ;
			}
		    }    
		}
	      else /* wanted movement opposite direction */
	      	{
		  // LDMotorOFF() ;
		  if(( ret=motor_off()) != SSD650V_STOPPED )
		    {
		      fprintf( stderr, "compare_azimuth: something went wrong with  azimuth motor (OFF)\n") ;
		    }
		  else
		    {
			  fprintf( stderr, "compare_azimuth: 21\n") ;
		    }
		  set_setpoint( curSetPoint) ;
		  fprintf( stderr, "Detected a sign change of Setpoint = %f, turning motor off and on\n", curSetPoint);

		  // LDMotorON() ;
		  if(( ret=motor_on()) != SSD650V_RUNNING )
		    {
		      fprintf( stderr, "compare_azimuth: something went wrong with  azimuth motor (ON)\n") ;
		      motor_on_off_state= MOTOR_UNDEFINED ;
		    }
		  else
		    {
		      fprintf( stderr, "compare_azimuth: 2\n") ;
		      motor_on_off_state= MOTOR_RUNNING ;
		    }

		}
	    }
	  else /* currently no movement */
	    {
	      set_setpoint( curSetPoint) ;

	      // motor not running
	      if( motor_on_off_state== MOTOR_NOT_RUNNING)
		{
		  // LDMotorON() ;
		  if(( ret=motor_on()) != SSD650V_RUNNING )
		    {
		      fprintf( stderr, "compare_azimuth: something went wrong with  azimuth motor (ON)\n") ;
		      motor_on_off_state= MOTOR_UNDEFINED ;
		    }
		  else
		    {
		      fprintf( stderr, "compare_azimuth: 3\n") ;
		      motor_on_off_state= MOTOR_RUNNING ;
		    }
		}
	    }
	  lastSetPoint= curSetPoint ;
	  lastSetPointSign= curSetPointSign ;
	}
      fprintf(stderr, "Sleeping---a-d:%5.4f----------------------------- s%3.1f, b:%5.3f, t:%5.6f, b-t:%6.4f\n", 180./M_PI*( .25 * AngularSpeed * (POLLMICROS/(1000. * 1000.))), curSetPoint, barcodereader_az, target_az, barcodereader_az- target_az) ;
      usleep(POLLMICROS) ;
    }
  return NULL ;
}

