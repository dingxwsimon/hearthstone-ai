#include <assert.h>
#include <iostream>

#include "engine/FlowControl/FlowController.h"
#include "engine/FlowControl/FlowController-impl.h"

class Test2_ActionParameterGetter : public engine::FlowControl::IActionParameterGetter
{
public:
	Test2_ActionParameterGetter() :
		next_defender_count(0),
		next_defender_idx(0),
		next_minion_put_location(0),
		next_specified_target_count(0),
		next_specified_target_idx(0)
	{}

	void SetMainOp(engine::MainOpType main_op) { main_op_ = main_op; }
	engine::MainOpType ChooseMainOp() { return main_op_; }

	void SetHandCard(int hand_card) { hand_card_ = hand_card; }
	int ChooseHandCard() { return hand_card_; }

	void SetAttacker(state::CardRef attacker) { attacker_ = attacker; }
	state::CardRef GetAttacker() { return attacker_; }

	state::CardRef GetDefender(std::vector<state::CardRef> const& targets)
	{
		assert(next_defender_count == (int)targets.size());
		assert(next_defender_idx >= 0);
		assert(next_defender_idx < (int)targets.size());
		return targets[next_defender_idx];
	}

	int GetMinionPutLocation(int minions)
	{
		return next_minion_put_location;
	}

	state::CardRef GetSpecifiedTarget(state::State & state, state::CardRef card_ref, std::vector<state::CardRef> const& targets)
	{
		assert((int)targets.size() == next_specified_target_count);

		if (next_specified_target_idx < 0) return state::CardRef();
		else return targets[next_specified_target_idx];
	}

	Cards::CardId ChooseOne(std::vector<Cards::CardId> const& cards) {
		assert(false);
		return cards[0];
	}

	engine::MainOpType main_op_;
	int hand_card_;
	state::CardRef attacker_;

	int next_defender_count;
	int next_defender_idx;

	int next_minion_put_location;

	int next_specified_target_count;
	int next_specified_target_idx;
};

class Test2_RandomGenerator : public engine::FlowControl::IRandomGenerator
{
public:
	Test2_RandomGenerator() :called(false), next_rand(0) {}

	int Get(int exclusive_max)
	{
		called = true;
		return next_rand;
	}

	size_t Get(size_t exclusive_max) { return (size_t)Get((int)exclusive_max); }

	int Get(int min, int max)
	{
		called = true;
		return min + next_rand;
	}

public:
	bool called;
	int next_rand;
};

static void PushBackDeckCard(Cards::CardId id, engine::FlowControl::FlowContext & flow_context, state::State & state, state::PlayerIdentifier player)
{
	int deck_count = (int)state.GetBoard().Get(player).deck_.Size();

	((Test2_RandomGenerator&)(flow_context.GetRandom())).next_rand = deck_count;
	((Test2_RandomGenerator&)(flow_context.GetRandom())).called = false;

	state.GetBoard().Get(player).deck_.ShuffleAdd(id, [&](int exclusive_max) {
		return flow_context.GetRandom().Get(exclusive_max);
	});

	if (deck_count > 0) assert(((Test2_RandomGenerator&)(flow_context.GetRandom())).called);
	++deck_count;

	assert(state.GetBoard().Get(player).deck_.Size() == deck_count);
}

static void MakeDeck(state::State & state, engine::FlowControl::FlowContext & flow_context, state::PlayerIdentifier player)
{
	PushBackDeckCard(Cards::ID_EX1_020, flow_context, state, player);
	PushBackDeckCard(Cards::ID_EX1_020, flow_context, state, player);
	PushBackDeckCard(Cards::ID_CS2_171, flow_context, state, player);
}

static state::Cards::Card CreateHandCard(Cards::CardId id, state::CardType type, state::State & state, state::PlayerIdentifier player)
{
	state::Cards::CardData raw_card = Cards::CardDispatcher::CreateInstance(id);
	assert(raw_card.card_type == type);

	raw_card.enchanted_states.player = player;
	raw_card.zone = state::kCardZoneNewlyCreated;
	raw_card.enchantment_handler.SetOriginalStates(raw_card.enchanted_states);

	return state::Cards::Card(raw_card);
}

static state::CardRef AddHandCard(Cards::CardId id, state::CardType type, engine::FlowControl::FlowContext & flow_context, state::State & state, state::PlayerIdentifier player)
{
	int hand_count = (int)state.GetBoard().Get(player).hand_.Size();

	auto ref = state.AddCard(CreateHandCard(id, type, state, player));
	state.GetZoneChanger<state::kCardZoneNewlyCreated>(ref)
		.ChangeTo<state::kCardZoneHand>(player);

	assert(state.GetCardsManager().Get(ref).GetCardId() == id);
	assert(state.GetCardsManager().Get(ref).GetPlayerIdentifier() == player);
	if (hand_count == 10) {
		assert(state.GetBoard().Get(player).hand_.Size() == 10);
		assert(state.GetCardsManager().Get(ref).GetZone() == state::kCardZoneGraveyard);
	}
	else {
		++hand_count;
		assert((int)state.GetBoard().Get(player).hand_.Size() == hand_count);
		assert(state.GetCardsManager().Get(ref).GetZone() == state::kCardZoneHand);
		if constexpr (!state::kOrderHandCardsByCardId) {
			assert(state.GetBoard().Get(player).hand_.Get(hand_count - 1) == ref);
			assert(state.GetCardsManager().Get(ref).GetZonePosition() == (hand_count - 1));
		}
	}

	return ref;
}

static void MakeHand(state::State & state, engine::FlowControl::FlowContext & flow_context, state::PlayerIdentifier player)
{
	AddHandCard(Cards::ID_EX1_089, state::kCardTypeMinion, flow_context, state, player);
	AddHandCard(Cards::ID_NEW1_038, state::kCardTypeMinion, flow_context, state, player);
	AddHandCard(Cards::ID_EX1_089, state::kCardTypeMinion, flow_context, state, player);
	AddHandCard(Cards::ID_EX1_089, state::kCardTypeMinion, flow_context, state, player);
	AddHandCard(Cards::ID_EX1_089, state::kCardTypeMinion, flow_context, state, player);
	AddHandCard(Cards::ID_NEW1_038, state::kCardTypeMinion, flow_context, state, player);
	AddHandCard(Cards::ID_NEW1_038, state::kCardTypeMinion, flow_context, state, player);
}

static void MakeHero(state::State & state, engine::FlowControl::FlowContext & flow_context, state::PlayerIdentifier player)
{
	state::Cards::CardData raw_card;
	raw_card.card_id = (Cards::CardId)8;
	raw_card.card_type = state::kCardTypeHero;
	raw_card.zone = state::kCardZoneNewlyCreated;
	raw_card.enchanted_states.max_hp = 30;
	raw_card.enchanted_states.player = player;
	raw_card.enchanted_states.attack = 0;
	raw_card.enchantment_handler.SetOriginalStates(raw_card.enchanted_states);

	state::CardRef ref = state.AddCard(state::Cards::Card(raw_card));

	state.GetZoneChanger<state::kCardTypeHero, state::kCardZoneNewlyCreated>(ref)
		.ChangeTo<state::kCardZonePlay>(player);


	auto hero_power = Cards::CardDispatcher::CreateInstance(Cards::ID_CS1h_001);
	assert(hero_power.card_type == state::kCardTypeHeroPower);
	hero_power.zone = state::kCardZoneNewlyCreated;
	ref = state.AddCard(state::Cards::Card(hero_power));
	state.GetZoneChanger<state::kCardTypeHeroPower, state::kCardZoneNewlyCreated>(ref)
		.ChangeTo<state::kCardZonePlay>(player);
}

struct MinionCheckStats
{
	int attack;
	int hp;
	int max_hp;
};

static void CheckMinion(state::State &state, state::CardRef ref, MinionCheckStats const& stats)
{
	assert(state.GetCardsManager().Get(ref).GetAttack() == stats.attack);
	assert(state.GetCardsManager().Get(ref).GetMaxHP() == stats.max_hp);
	assert(state.GetCardsManager().Get(ref).GetHP() == stats.hp);
}

static void CheckMinions(state::State & state, state::PlayerIdentifier player, std::vector<MinionCheckStats> const& checking)
{
	std::vector<state::CardRef> const& minions = state.GetBoard().Get(player).minions_.Get();

	assert(minions.size() == checking.size());
	for (size_t i = 0; i < minions.size(); ++i) {
		CheckMinion(state, minions[i], checking[i]);
	}
}

struct CrystalCheckStats
{
	int current;
	int total;
};
static void CheckCrystals(state::State & state, state::PlayerIdentifier player, CrystalCheckStats checking)
{
	assert(state.GetBoard().Get(player).GetResource().GetCurrent() == checking.current);
	assert(state.GetBoard().Get(player).GetResource().GetTotal() == checking.total);
}

static void CheckHero(state::State & state, state::PlayerIdentifier player, int hp, int armor, int attack)
{
	auto hero_ref = state.GetBoard().Get(player).GetHeroRef();
	auto const& hero = state.GetCardsManager().Get(hero_ref);
	(void)hero;

	assert(hero.GetHP() == hp);
	assert(hero.GetArmor() == armor);
	assert(hero.GetAttack() == attack);
}

void test2()
{
	Test2_ActionParameterGetter parameter_getter;
	Test2_RandomGenerator random;
	state::State state;
	engine::FlowControl::FlowContext flow_context(random, parameter_getter);

	engine::FlowControl::FlowController controller(state, flow_context);

	MakeHero(state, flow_context, state::PlayerIdentifier::First());
	MakeDeck(state, flow_context, state::PlayerIdentifier::First());
	MakeHand(state, flow_context, state::PlayerIdentifier::First());

	MakeHero(state, flow_context, state::PlayerIdentifier::Second());
	state.GetBoard().Get(state::PlayerIdentifier::Second()).SetFatigueDamage(3);
	MakeDeck(state, flow_context, state::PlayerIdentifier::Second());
	MakeHand(state, flow_context, state::PlayerIdentifier::Second());

	state.GetMutableCurrentPlayerId().SetFirst();
	state.GetBoard().Get(state::PlayerIdentifier::First()).GetResource().SetTotal(8);
	state.GetBoard().Get(state::PlayerIdentifier::First()).GetResource().Refill();
	state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().SetTotal(4);

	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 8,8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0,4 });
	CheckMinions(state, state::PlayerIdentifier::First(), {});
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(0);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {4, 4, 4} });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});

	state.GetBoard().Get(state::PlayerIdentifier::First()).GetResource().Refill();

	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 8, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	parameter_getter.next_minion_put_location = 1;
	random.next_rand = 0;
	random.called = false;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(0);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(!random.called);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 0, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 }, {7,7,7} });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});

	{
		state::State state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);

		parameter_getter.next_defender_count = 1;
		parameter_getter.next_defender_idx = 0;
		assert(!engine::FlowControl::ValidActionGetter(state2).IsAttackable(
			state2.GetBoard().Get(state::PlayerIdentifier::First()).minions_.Get(0)
		));
	}

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 0, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 8,8,8 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetCurrentPlayerId() == state::PlayerIdentifier::Second());

	{
		state::State state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);

		parameter_getter.next_defender_count = 3;
		parameter_getter.next_defender_idx = 0;
		assert(!engine::FlowControl::ValidActionGetter(state2).IsAttackable(
			state2.GetBoard().Get(state::PlayerIdentifier::First()).minions_.Get(0)
		));
	}

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(state.GetCurrentPlayerId() == state::PlayerIdentifier::First());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 9,9,9 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});

	parameter_getter.next_defender_count = 1;
	parameter_getter.next_defender_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.SetAttacker(state.GetBoard().Get(state::PlayerIdentifier::First()).minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 9,9,9 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetCurrentPlayerId() == state::PlayerIdentifier::First());
	assert(state.GetCurrentPlayer().hand_.Size() == 6);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 8);

	AddHandCard(Cards::ID_CS2_189, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::Second());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 9, 9, 9 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetCurrentPlayerId() == state::PlayerIdentifier::First());
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 9);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);

	AddHandCard(Cards::ID_CS2_189, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::Second());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 9, 9, 9 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetCurrentPlayerId() == state::PlayerIdentifier::First());
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 10);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(state.GetCurrentPlayerId() == state::PlayerIdentifier::Second());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 10, 10, 10 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetCurrentPlayer().hand_.Size() == 10);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);

	assert(state.GetCardsManager().Get(state.GetCurrentPlayer().hand_.Get(9)).GetCardId() == Cards::ID_CS2_189);
	parameter_getter.next_specified_target_count = 4;
	parameter_getter.next_specified_target_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(9);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 10, 9, 10 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { {1, 1, 1} });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);

	AddHandCard(Cards::ID_EX1_508, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::Second());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 10, 9, 10 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 10);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);
	assert(state.GetCardsManager().Get(state.GetCurrentPlayer().hand_.Get(9)).GetCardId() == Cards::ID_EX1_508);

	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(9);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 10, 9, 10 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 }, {1, 1, 1} });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);

	AddHandCard(Cards::ID_CS2_168, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::Second());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 10, 9, 10 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 }, {1, 1, 1} });
	assert(state.GetCurrentPlayer().hand_.Size() == 10);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);
	assert(state.GetCardsManager().Get(state.GetCurrentPlayer().hand_.Get(9)).GetCardId() == Cards::ID_CS2_168);

	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(9);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 10, 9, 10 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 }, {3, 1, 1}, { 1, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 8, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 12, 11, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 3, 1, 1 }, {1, 1, 1} });
	assert(state.GetCurrentPlayer().hand_.Size() == 10);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 0);

	parameter_getter.next_defender_count = 3;
	parameter_getter.next_defender_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.SetAttacker(state.GetCurrentPlayer().minions_.Get(2));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 8, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 12, 10, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 2, 1, 1 }});
	assert(state.GetCurrentPlayer().hand_.Size() == 10);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).graveyard_.GetTotalMinions() == 1);

	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(7);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 4, 4 },{ 12, 10, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 }, {1, 1, 1}, { 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);

	parameter_getter.next_defender_count = 3;
	parameter_getter.next_defender_idx = 1;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.SetAttacker(state.GetCurrentPlayer().minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 3, 4 },{ 12, 10, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);

	AddHandCard(Cards::ID_EX1_011, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::Second());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 3, 4 }, { 12, 10, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 }, { 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 10);

	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 6;
	parameter_getter.next_specified_target_idx = 3;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(9);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 3, 4 },{ 12, 10, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { {2, 1, 1}, { 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);

	AddHandCard(Cards::ID_CS2_106, state::kCardTypeWeapon, flow_context, state, state::PlayerIdentifier::Second());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 3, 4 },{ 12, 10, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 10);

	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(9);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 3, 4 },{ 12, 10, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(state.GetCurrentPlayer().GetWeaponRef().IsValid());

	parameter_getter.next_defender_count = 3;
	parameter_getter.next_defender_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.SetAttacker(state.GetCurrentPlayer().GetHeroRef());
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 11, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 3, 4 },{ 12, 7, 12 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(state.GetCurrentPlayer().GetWeaponRef().IsValid());
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 3);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 4);
	CheckHero(state, state::PlayerIdentifier::Second(), 7, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 3, 4 }, { 14, 9, 14 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(state.GetCurrentPlayer().GetWeaponRef().IsValid());

	parameter_getter.next_defender_count = 3;
	parameter_getter.next_defender_idx = 1;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.SetAttacker(state.GetCurrentPlayer().GetHeroRef());
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), {{ 14, 9, 14 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	AddHandCard(Cards::ID_CS2_106, state::kCardTypeWeapon, flow_context, state, state::PlayerIdentifier::Second());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 14, 9, 14 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 10);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(9);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 14, 9, 14 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(state.GetCurrentPlayer().GetWeaponRef().IsValid());

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 1);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 15, 10, 15 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 8);
	assert(state.GetBoard().GetSecond().GetWeaponRef().IsValid());

	AddHandCard(Cards::ID_EX1_066, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::First());
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 1);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 15, 10, 15 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(state.GetBoard().GetSecond().GetWeaponRef().IsValid());

	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(8);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 1);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 8, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 15, 10, 15 }, {3, 2, 2} });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 8);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	AddHandCard(Cards::ID_CS2_142, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::First());
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 1);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 8, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 15, 10, 15 },{ 3, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	parameter_getter.next_minion_put_location = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(8);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 4);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 6, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 15, 10, 15 },{ 3, 2, 2 }, {2, 2, 2} });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 8);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	AddHandCard(Cards::ID_EX1_506, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::First());
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 4);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 6, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 15, 10, 15 },{ 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(8);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 4);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 4, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 15, 10, 15 }, {2, 1, 1}, {1, 1, 1}, { 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 8);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	AddHandCard(Cards::ID_EX1_015, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::First());
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 4);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 4, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 15, 10, 15 },{ 2, 1, 1 },{ 1, 1, 1 },{ 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(8);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 4);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {1, 1, 1}, { 15, 10, 15 },{ 2, 1, 1 },{ 1, 1, 1 },{ 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 8);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	AddHandCard(Cards::ID_EX1_506, state::kCardTypeMinion, flow_context, state, state::PlayerIdentifier::First());
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 4);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 15, 10, 15 },{ 2, 1, 1 },{ 1, 1, 1 },{ 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 9);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());

	parameter_getter.next_minion_put_location = 4;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(8);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).GetFatigueDamage() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetFatigueDamage() == 4);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 3, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 0, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 15, 10, 15 },{ 2, 1, 1 },{ 1, 1, 1 },{2, 1, 1}, { 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	assert(state.GetCurrentPlayer().hand_.Size() == 8);
	assert(!state.GetCurrentPlayer().GetWeaponRef().IsValid());
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	assert(controller.PerformAction() == engine::kResultFirstPlayerWin);
}