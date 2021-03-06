#pragma once

#include "Utils/StaticInvokables.h"
#include "state/State.h"

namespace Cards
{
	// event lifetime
	struct MinionInPlayZone {
		template <typename YesFunctor, typename NoFunctor> using AddedToPlayZone = YesFunctor;
		template <typename YesFunctor, typename NoFunctor> using AddedToHandZone = NoFunctor;

		static bool StillValid(state::Cards::Card const& card) {
			if (card.GetZone() != state::kCardZonePlay) return false;
			if (card.IsSilenced()) return false;
			return true;
		}
	};
	struct SecretInPlayZone {
		template <typename YesFunctor, typename NoFunctor> using AddedToPlayZone = YesFunctor;
		template <typename YesFunctor, typename NoFunctor> using AddedToHandZone = NoFunctor;

		static bool StillValid(state::Cards::Card const& card) {
			return card.GetZone() == state::kCardZonePlay;
		}
	};
	struct WeaponInPlayZone {
		template <typename YesFunctor, typename NoFunctor> using AddedToPlayZone = YesFunctor;
		template <typename YesFunctor, typename NoFunctor> using AddedToHandZone = NoFunctor;

		static bool StillValid(state::Cards::Card const& card) {
			return card.GetZone() == state::kCardZonePlay;
		}
	};
	struct InHandZone {
		template <typename YesFunctor, typename NoFunctor> using AddedToPlayZone = NoFunctor;
		template <typename YesFunctor, typename NoFunctor> using AddedToHandZone = YesFunctor;

		static bool StillValid(state::Cards::Card const& card) {
			if (card.GetZone() != state::kCardZoneHand) return false;
			return true;
		}
	};

	// self policy
	// Note: we need to remember 'self' to check if owner is still alive (depends on lifetime policy)
	struct NonCategorized_SelfInLambdaCapture {}; // remember self on lambda capture
	struct CateogrizedOnSelf {}; // remember self on categorized event

	// helper
	namespace detail {
		template <typename EventHandler, typename EventHandlerArg> struct EventHandlerInvoker {
			template <typename... Args> static auto Invoke(Args&&... args) {
				return EventHandler::template HandleEvent<EventHandlerArg>(std::forward<Args>(args)...);
			}
		};
		template <typename EventHandler> struct EventHandlerInvoker<EventHandler, void> {
			template <typename... Args> static auto Invoke(Args&&... args) {
				return EventHandler::HandleEvent(std::forward<Args>(args)...);
			}
		};

		struct ContextCardGetter {
			template <class Context>
			static auto const& Get(Context const& context, state::CardRef ref) { return context.manipulate_.Board().GetCard(ref); }
		};

		// Note: if more than one event types need special care,
		// use SFINAE to let compiler do the job.
		template <>
		auto const& ContextCardGetter::Get(state::Events::EventTypes::GetPlayCardCost::Context const& context, state::CardRef ref) {
			return context.state_.GetCard(ref);
		}

		template <typename LifeTime, typename SelfPolicy, typename EventType, typename EventHandler, typename EventHandlerArg> struct AddEventHelper;

		template <typename LifeTime, typename EventType, typename EventHandler, typename EventHandlerArg>
		struct AddEventHelper<LifeTime, NonCategorized_SelfInLambdaCapture, EventType, EventHandler, EventHandlerArg> {
			static void AddEvent(state::CardRef self, state::Cards::ZoneChangedContext const& context) {
				context.state_.AddEvent<EventType>(
					[self](auto context) {
					if (!LifeTime::StillValid(ContextCardGetter::Get(context, self))) return false;
					return EventHandlerInvoker<EventHandler, EventHandlerArg>::Invoke(self, context);
				});
			}
		};
		template <typename LifeTime, typename EventType, typename EventHandler, typename EventHandlerArg>
		struct AddEventHelper<LifeTime, CateogrizedOnSelf, EventType, EventHandler, EventHandlerArg> {
			static void AddEvent(state::CardRef self, state::Cards::ZoneChangedContext const& context) {
				context.state_.AddEvent<EventType>(
					self,
					[](state::CardRef self, auto context) {
					if (!LifeTime::StillValid(context.manipulate_.Board().GetCard(self))) return false;
					return EventHandlerInvoker<EventHandler, EventHandlerArg>::Invoke(self, context);
				});
			}
		};

		struct NullEventRegisterHelper {
			struct Invoker {
				template <typename Context> static void Invoke(Context context) {
					return;
				}
			};
			using AddedToPlayZone = Utils::StaticInvokableChain<Invoker>;
			using AddedToHandZone = Utils::StaticInvokableChain<Invoker>;
		};
	}

	template <typename LifeTime, typename SelfPolicy, typename EventType, typename EventHandler, typename EventHandlerArg = void>
	struct OneEventRegisterHelper {
		struct Invoker {
			template <typename Context>
			static void Invoke(Context&& context) {
				state::CardRef self = context.card_ref_;
				detail::AddEventHelper<LifeTime, SelfPolicy, EventType, EventHandler, EventHandlerArg>
					::AddEvent(self, std::forward<Context>(context));
			}
		};
		struct NullInvoker {
			template <typename Context> static void Invoke(Context context) {}
		};

		struct AddedToPlayZone {
			template <typename Context>
			static void Invoke(Context context) {
				return LifeTime::template AddedToPlayZone<Invoker, NullInvoker>
					::Invoke(std::forward<Context>(context));
			}
		};

		struct AddedToHandZone {
			template <typename Context>
			static void Invoke(Context context) {
				return LifeTime::template AddedToHandZone<Invoker, NullInvoker>
					::Invoke(std::forward<Context>(context));
			}
		};
	};

	template <typename FirstEvent, typename SecondEvent>
	struct CombineEvents {
		struct AddedToPlayZone {
			template <typename Context>
			static void Invoke(Context&& context) {
				FirstEvent::AddedToPlayZone::Invoke(std::forward<Context>(context));
				SecondEvent::AddedToPlayZone::Invoke(std::forward<Context>(context));
			}
		};
		struct AddedToHandZone {
			template <typename Context>
			static void Invoke(Context&& context) {
				FirstEvent::AddedToHandZone::Invoke(std::forward<Context>(context));
				SecondEvent::AddedToHandZone::Invoke(std::forward<Context>(context));
			}
		};
	};

	template <typename... Events> struct EventsRegisterHelper;

	template <typename Event1, typename Event2, typename... RestEvents>
	struct EventsRegisterHelper<Event1, Event2, RestEvents...> {
		static void Process(state::Cards::CardData & card_data) {
			using CombinedEvent = CombineEvents<Event1, Event2>;
			return EventsRegisterHelper<CombinedEvent, RestEvents...>::Process(card_data);
		}
	};

	template <typename Event1>
	struct EventsRegisterHelper<Event1> {
		static void Process(state::Cards::CardData & card_data) {
			using Event2 = detail::NullEventRegisterHelper;
			return EventsRegisterHelper<Event1, Event2>::Process(card_data);
		}
	};

	template <typename Event1, typename Event2>
	struct EventsRegisterHelper<Event1, Event2> {
		static void Process(state::Cards::CardData & card_data) {
			using CombinedEvent = CombineEvents<Event1, Event2>;
			card_data.added_to_play_zone += (CombinedEvent::AddedToPlayZone::Invoke);
			card_data.added_to_hand_zone += (CombinedEvent::AddedToHandZone::Invoke);
		}
	};

	// event register: use helper for common usages
	template <typename EventType, typename EventHandler = void>
	using ManagedOneEventRegisterHelper = OneEventRegisterHelper<
		typename EventType::LifeTime,
		typename EventType::SelfPolicy,
		typename EventType::EventType,
		typename EventType::EventHandler,
		EventHandler>;

	// helpers for common usages
	struct OnSelfTakeDamage {
		using LifeTime = MinionInPlayZone;
		using SelfPolicy = CateogrizedOnSelf;
		using EventType = state::Events::EventTypes::CategorizedOnTakeDamage;

		struct EventHandler {
			template <typename UnderlyingHandler>
			static bool HandleEvent(state::CardRef self, EventType::Context const& context) {
				assert(*context.damage_ > 0);
				return UnderlyingHandler::HandleEvent(self, context);
			}
		};
	};
}