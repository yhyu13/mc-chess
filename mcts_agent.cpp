#include "mcts_agent.hpp"
#include "mcts.hpp"

MCTSAgent::MCTSAgent(unsigned nponderers)
  : pending_change(false),
    barrier_before_change(nponderers + 1),
    barrier_after_change(nponderers + 1),
    do_ponder(false),
    do_terminate(false),
    node(nullptr)
{
  for (unsigned i = 0; i < nponderers; i++) {
    auto seed = generator();
    ponderers.create_thread([this, seed](){
        boost::mt19937 generator(seed);
        ponder(generator);
      });
  }
}

MCTSAgent::~MCTSAgent() {
  do_terminate = true;
  ponderers.join_all();
}

void MCTSAgent::between_ponderings(std::function<void()> change) {
  pending_change = true;
  barrier_before_change.wait();
  change();
  pending_change = false;
  barrier_after_change.wait();
}

void MCTSAgent::perform_pondering(std::function<void()> pondering) {
  if (pending_change || !do_ponder) {
    barrier_before_change.wait();
    barrier_after_change.wait();
    assert(node || !do_ponder);
  }
  if (do_ponder)
    pondering();
}

void MCTSAgent::set_state(State state) {
  if (state == this->state)
    return;
  between_ponderings([this, state]() {
      this->state = state;
      if (this->node)
        delete this->node;
      this->node = new mcts::Node(nullptr, boost::none, state);
    });
}

void MCTSAgent::advance_state(Move move) {
  assert(this->state);
  assert(this->node);
  between_ponderings([this, move]() {
      this->state->make_move(move);
      mcts::Node* child = this->node->get_child(move);
      if (child) {
        this->node = child->destroy_parent_and_siblings();
        assert(this->node);
      } else {
        // no simulations between set_state and advance_state, so nothing thrown
        // away by just replacing the node.
        delete this->node;
        this->node = new mcts::Node(nullptr, move, *state);
      }
    });
}

void MCTSAgent::start_pondering() {
  between_ponderings([this]() {
      do_ponder = true;
    });
}

void MCTSAgent::stop_pondering() {
  between_ponderings([this]() {
      do_ponder = false;
    });
}

void MCTSAgent::ponder(boost::mt19937 generator) {
  while (!do_terminate) {
    perform_pondering([this, &generator]() {
        for (int i = 0; i < 100; i++)
          node->sample(*state, generator);
      });
  }
}

Move MCTSAgent::decide() {
  throw std::runtime_error("not implemented");
}

boost::future<Move> MCTSAgent::start_decision() {
  start_pondering();
  return boost::async([this]() {
      boost::this_thread::sleep_for(boost::chrono::seconds(5));
      node->print_statistics();
      return node->best_move();
    });
}
void MCTSAgent::finalize_decision() {
  // FIXME
}
void MCTSAgent::abort_decision() {
  // FIXME
}

bool MCTSAgent::accept_draw(Color color) {
  static boost::random::bernoulli_distribution<> distribution(0.1);
  return distribution(generator);
}

void MCTSAgent::idle() {
  abort_decision();
  stop_pondering();
}

void MCTSAgent::pause() {
  stop_pondering();
}
void MCTSAgent::resume() {
  start_pondering();
}
