#pragma once

#include <utility>

#include "Utils/CloneableContainers/Vector.h"
#include "state/Cards/Card.h"
#include "state/Types.h"

namespace state
{
	class State;

	namespace Cards
	{
		class Manager
		{
		public:
			typedef Utils::CloneableContainers::Vector<Card> ContainerType;

			Card const& Get(CardRef id) const { return cards_.Get(id.id); }
			Card & GetMutable(CardRef id) { return cards_.Get(id.id); }

			CardRef PushBack(Cards::Card&& card);

			void SetCardZonePos(CardRef ref, int pos)
			{
				GetMutable(ref).SetZonePos()(pos);
			}

		private:
			ContainerType cards_;
		};
	}
}