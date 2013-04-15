/*
 Copyright (C) 2009 Erik Hjortsberg <erik.hjortsberg@gmail.com>

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

#ifndef EMBEROGRETERRAINTERRAINAREAUPDATETASK_H_
#define EMBEROGRETERRAINTERRAINAREAUPDATETASK_H_
#include "TerrainAreaTaskBase.h"

#include <wfmath/point.h>
#include <wfmath/axisbox.h>

namespace Ember
{
namespace OgreView
{

namespace Terrain
{
class TerrainShader;

/**
 * @author Erik Hjortsberg <erik.hjortsberg@gmail.com>
 * @brief Updates terrain areas.
 */
class TerrainAreaUpdateTask: public TerrainAreaTaskBase
{
public:

	/**
	 * @brief Ctor.
	 * @param terrain The terrain.
	 * @param area The terrain area which is updated.
	 * @param shader The affected shader.
	 * @param markForUpdateSlot A slot which will be called in the main thread when the update is complete.
	 * @param oldShape The old shape, before the update.
	 */
	TerrainAreaUpdateTask(Mercator::Terrain& terrain, Mercator::Area* area, ShaderUpdateSlotType markForUpdateSlot, const TerrainShader* shader, const WFMath::AxisBox<2>& oldShape);
	virtual ~TerrainAreaUpdateTask();

	virtual void executeTaskInBackgroundThread(Tasks::TaskExecutionContext& context);

	virtual void executeTaskInMainThread();

private:

	/**
	 * @brief The terrain shader affected.
	 */
	const TerrainShader* mShader;

	const WFMath::AxisBox<2> mOldShape;

};

}
}
}
#endif /* EMBEROGRETERRAINTERRAINAREAUPDATETASK_H_ */
