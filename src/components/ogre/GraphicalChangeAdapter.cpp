/*
 Copyright (C) 2012 Erik Ogenvik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "GraphicalChangeAdapter.h"

namespace Ember
{
namespace OgreView
{

bool GraphicalChangeAdapter::fpsChangeRequired(float changeSize)
{
	//for now leaving it at this, need to update later with better calibrated values
	float translatedChangeRequired = changeSize / 1.0f;

	bool furtherChangePossible = EventChangeRequired.emit(translatedChangeRequired);
	return furtherChangePossible;
}
}
}
