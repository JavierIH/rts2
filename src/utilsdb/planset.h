/* 
 * Plan set class.
 * Copyright (C) 2003-2007 Petr Kubanek <petr@kubanek.net
 * Copyright (C) 2011 Petr Kubanek, Institute of Physics <kubanek.net>
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

#ifndef __RTS2_PLANSET__
#define __RTS2_PLANSET__

#include "plan.h"

#include <list>
#include <ostream>
#include <string>
#include <time.h>

namespace rts2db
{

/**
 * List of plan entries, selected from various
 * filtering criteria.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class PlanSet:public std::list < Plan >
{
	public:
		PlanSet ();
		PlanSet (int prop_id);
		PlanSet (time_t * t_from, time_t * t_to);
		PlanSet (double t_from, double t_to);
		virtual ~PlanSet (void);

		void load ();

		friend std::ostream & operator << (std::ostream & _os, PlanSet & plan_set)
		{
			PlanSet::iterator plan_iter;
			for (plan_iter = plan_set.begin (); plan_iter != plan_set.end (); plan_iter++)
			{
				_os << &(*plan_iter);
			}
			return _os;
		}

	protected:
		std::string where;
};

/**
 * Set of plan entries for given target.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class PlanSetTarget:public PlanSet
{
	public:
		PlanSetTarget (int tar_id);
};

}

#endif							 /* !__RTS2_PLANSET__ */
