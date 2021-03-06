#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "state/State.h"
#include "MCTS/MOMCTS.h"
#include "judge/IAgent.h"
#include "agents/MCTSRunner.h"

namespace agents
{
	template <class IterationCallback>
	class MCTSAgent {
	public:
		MCTSAgent(int threads, int tree_samples) :
			threads_(threads),
			tree_samples_(tree_samples),
			root_node_(nullptr), node_(nullptr), controller_(),
			iteration_cb_()
		{}

		MCTSAgent(MCTSAgent const&) = delete;
		MCTSAgent & operator=(MCTSAgent const&) = delete;

		template <class... Args>
		void SetupIterationCallback(Args&&... args) {
			this->iteration_cb_.Initialize(std::forward<Args>(args)...);
		}

		template <class StateGetter>
		void Think(state::PlayerIdentifier side, StateGetter && state_getter, std::mt19937 & random) {
			auto continue_checker = [&]() {
				uint64_t iterations = controller_->GetStatistic().GetSuccededIterates();
				return iteration_cb_(std::forward<StateGetter>(state_getter), iterations);
			};

			controller_.reset(new MCTSRunner(tree_samples_, random));
			controller_->Run(threads_, std::forward<StateGetter>(state_getter));

			while (true) {
				if (!continue_checker()) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			controller_->WaitUntilStopped();

			node_ = controller_->GetRootNode(side);
			root_node_ = node_;
		}

		int GetAction(engine::ActionType::Types action_type, engine::ActionChoices action_choices) {
			if (action_type != engine::ActionType::kMainAction)
			{
				assert(action_choices.Size() > 0);
				if (action_choices.Size() == 1) return 0;
			}

			assert(node_);

			if (node_->GetActionType() != action_type) {
				assert(false);
				throw std::runtime_error("Action type not match");
			}

			auto CanBeChosen = [&](int choice) {
				for (action_choices.Begin(); !action_choices.IsEnd(); action_choices.StepNext()) {
					if (action_choices.Get() == choice) return true;
				}
				return false;
			};

			int best_choice = -1;
			double best_chosen_times = -std::numeric_limits<double>::infinity();
			mcts::builder::TreeBuilder::TreeNode const* best_node = nullptr;
			node_->ForEachChild([&](int choice, mcts::selection::ChildType const& child) {
				if (!CanBeChosen(choice)) return true;

				double chosen_times = (double)child.GetEdgeAddon().GetChosenTimes();
				if (chosen_times > best_chosen_times) {
					best_chosen_times = chosen_times;
					best_choice = choice;
					best_node = child.GetNode();
				}
				return true;
			});

			if (best_choice < 0) {
				throw std::runtime_error("No any choice is evaluated.");
			}

			node_ = best_node;
			return best_choice;
		}

	private:
		int threads_;
		int tree_samples_;
		mcts::builder::TreeBuilder::TreeNode const* root_node_;
		mcts::builder::TreeBuilder::TreeNode const* node_;
		std::unique_ptr<MCTSRunner> controller_;
		IterationCallback iteration_cb_;
	};
}