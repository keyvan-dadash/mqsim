#ifndef RL_H_
#define RL_H_

#include <random>
#include <chrono>
#include <vector>

// TODO: we should define this in another way
#define W 750000

enum ActionE
{
    WAIT = 0,
    NO_WAIT,

    MAX_ACTIONS
};

enum CurrentInterval
{
    C_UNDER_HALF_W = 0,
    C_UPPER_HALF_W,

    C_MAX_CURRENT_INT
};

enum PreviousInterval
{
    P_UNDER_QUARTER_W = 0,
    P_BETWEEN_QUARTER_AND_HALF_W,
    P_UPPER_HALF_W,

    P_MAX_PREV_INT
};

struct State
{
    CurrentInterval current_int;
    PreviousInterval previous_int;

    static State GetStateFrom(int64_t current_interval, int64_t previous_interval)
    {
        CurrentInterval current_int;
        PreviousInterval previous_int;

        if (current_interval > W/2)
            current_int = CurrentInterval::C_UPPER_HALF_W;
        else
            current_int = CurrentInterval::C_UNDER_HALF_W;
        
        if (previous_interval > W/2)
            previous_int = PreviousInterval::P_UPPER_HALF_W;
        else if (previous_interval > W/4 && previous_interval < W/2)
            previous_int = PreviousInterval::P_BETWEEN_QUARTER_AND_HALF_W;
        else
            previous_int = PreviousInterval::P_UNDER_QUARTER_W;

        return State
        {
            .current_int = current_int,
            .previous_int = previous_int
        };
    }
};

struct Action
{
    ActionE action;

    static bool IsActionToWait(Action action)
    {
        if (action.action == ActionE::NO_WAIT)
            return false;
        return true;
    }
};

struct StateActionSpace
{
    State states;
    Action actions;
};

class Agent
{
  public:

    Agent();

    Action chonseAction(State state);

    void updateQ(State init_state, State next_state, double reward, Action action);

    void print_qtable_debug();

  private:

    double q_table_[P_MAX_PREV_INT * C_MAX_CURRENT_INT][MAX_ACTIONS];

    double epsilon = 0.2;

    double gamma = 1.0;

    double alpha = 0.99;

    std::mt19937_64 rng;

    Action getMaxQ(State state);

    std::vector<double> getNRandomNumber(int n, int start, int end);
};

#endif /* RL_H_ */