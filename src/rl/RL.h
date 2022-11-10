#ifndef RL_H_
#define RL_H_

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
    UNDER_HALF_W = 0,
    UPPER_HALF_W,

    MAX_CURRENT_INT
};

enum PreviousInterval
{
    UNDER_QUARTER_W = 0,
    BETWEEN_QUARTER_AND_HALF_W,
    UPPER_HALF_W,

    MAX_PREV_INT
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
            current_int = CurrentInterval::UPPER_HALF_W;
        else
            current_int = CurrentInterval::UNDER_HALF_W;
        
        if (previous_interval > W/2)
            previous_int = PreviousInterval::UPPER_HALF_W;
        else if (previous_int > W/4 && previous_int < W/2)
            previous_int = PreviousInterval::BETWEEN_QUARTER_AND_HALF_W;
        else
            previous_int = PreviousInterval::UNDER_QUARTER_W;

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

    Action chonseAction(State state);

    void updateQ(State init_state, State next_state, int reward, Action action);

  private:

    double q_table_[MAX_PREV_INT * MAX_CURRENT_INT][MAX_ACTIONS];

    Action getMaxQ(State state);
};

#endif /* RL_H_ */