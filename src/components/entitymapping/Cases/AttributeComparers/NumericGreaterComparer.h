//
// C++ Interface: NumericGreaterComparer
//
// Description:
//
//
// Author: Erik Hjortsberg <erik.hjortsberg@gmail.com>, (C) 2007
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.//
//
#ifndef EMBEROGRE_MODEL_MAPPING_CASES_ATTRIBUTECOMPARERSNUMERICGREATERCOMPARER_H
#define EMBEROGRE_MODEL_MAPPING_CASES_ATTRIBUTECOMPARERSNUMERICGREATERCOMPARER_H

#include "NumericComparer.h"
#include "NumericValueHolder.h"


namespace Ember {



namespace EntityMapping {

namespace Cases {

namespace AttributeComparers {

/**
	Test for greater.
	@author Erik Hjortsberg <erik.hjortsberg@gmail.com>
*/
class NumericGreaterComparer : public NumericComparer, private NumericValueHolder
{
public:
	NumericGreaterComparer(float value);
	virtual ~NumericGreaterComparer() {}
	/**
	Returns true if the supplied value is greater than the held value.
	*/
	virtual bool test(float value);

protected:
};

}

}

}

}

#endif
