#pragma once

#include "Actor.h"

//Debug representation of bounding sphere for actors.
class DebugSphere : public ActorSystem
{
public:
	DebugSphere();
	virtual void Tick(float deltaTime);
};