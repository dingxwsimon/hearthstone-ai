#pragma once

#include <array>
#include <vector>

#include "state/State.h"
#include "engine/MainOp.h"

namespace engine {
	namespace FlowControl
	{
		class ValidActionGetter;
	}

	class ValidActionAnalyzer
	{
	public:
		ValidActionAnalyzer() : op_map_(), op_map_size_(0), attackers_(), attacker_indics_(), playable_cards_() {}

		ValidActionAnalyzer(ValidActionAnalyzer const& rhs) :
			op_map_(rhs.op_map_),
			op_map_size_(rhs.op_map_size_),
			attackers_(rhs.attackers_),
			attacker_indics_(rhs.attacker_indics_),
			playable_cards_(rhs.playable_cards_)
		{}

		ValidActionAnalyzer & operator=(ValidActionAnalyzer const& rhs) {
			op_map_ = rhs.op_map_;
			op_map_size_ = rhs.op_map_size_;
			attackers_ = rhs.attackers_;
			attacker_indics_ = rhs.attacker_indics_;
			playable_cards_ = rhs.playable_cards_;
			return *this;
		}

		void Reset() { op_map_size_ = 0; }

		void Analyze(state::State const& state);
		void Analyze(FlowControl::ValidActionGetter const& getter);

		auto const& GetMainActions() const { return op_map_; }
		int GetMainActionsCount() const { return (int)op_map_size_; }
		auto const& GetAttackers() const { return attackers_; }
		auto const& GetAttackerIndics() const { return attacker_indics_; }
		auto const& GetPlayableCards() const { return playable_cards_; }

		template <class Functor>
		void ForEachMainOp(Functor && functor) const {
			for (size_t i = 0; i < op_map_size_; ++i) {
				if (!functor(i, GetMainOpType(op_map_[i]))) return;
			}
		}
		engine::MainOpType GetMainOpType(size_t choice) const {
			return op_map_[choice];
		}
		template <class Functor>
		void ForEachPlayableCard(Functor && functor) const {
			for (auto hand_idx : playable_cards_) {
				if (!functor(hand_idx)) break;
			}
		}
		size_t GetPlaybleCard(size_t idx) const {
			return playable_cards_[idx];
		}

		int GetEncodedAttackerIndex(size_t idx) const {
			return attackers_[idx];
		}

	private:
		std::array<engine::MainOpType, engine::MainOpType::kMainOpMax> op_map_;
		size_t op_map_size_;
		std::vector<int> attackers_;
		std::array<state::CardRef, 8> attacker_indics_;
		std::vector<size_t> playable_cards_;
	};
}