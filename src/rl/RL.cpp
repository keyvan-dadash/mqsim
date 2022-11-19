#include <assert.h>
#include <iostream>
#include <chrono>
#include <random>
#include <vector>

#include "./RL.h"

Agent::Agent()
{
    for (int i = 0; i < C_MAX_CURRENT_INT * P_MAX_PREV_INT; i++) {
        for (int j = 0; j < MAX_ACTIONS; j++) {
            q_table_[i][j] = 0;
        }
    }
}

Action Agent::chonseAction(State state)
{
    auto rand_number_vec = getNRandomNumber(1, 0, 1);
    double random_number = rand_number_vec.at(0);

    if (random_number < epsilon) {
        rand_number_vec = getNRandomNumber(1, 0, MAX_ACTIONS);
        int action = std::floor(rand_number_vec.at(0));
        ActionE random_action = static_cast<ActionE>(action);
        return Action {
            .action = random_action
        };
    } 

    return getMaxQ(state);
}

void Agent::updateQ(State init_state, State next_state, double reward, Action action)
{
    auto init_state_actions_value = q_table_[(init_state.current_int + 1) * (init_state.previous_int + 1)];
    auto init_action_value = init_state_actions_value[static_cast<int>(action.action)];
    init_state_actions_value[static_cast<int>(action.action)] = init_action_value + \
        alpha * (reward + gamma * static_cast<double>(getMaxQ(next_state).action) - init_action_value);
    alpha *= 0.999999;
    std::cout << init_state_actions_value[static_cast<int>(action.action)] << std::endl;
}

Action Agent::getMaxQ(State state)
{
    auto actions_value = q_table_[(state.current_int + 1) * (state.previous_int + 1)];
    
    double max = -9999999;
    int action = -1;
    for (int i = 0; i < MAX_ACTIONS; i++) {
        if (max < actions_value[i])
            max = actions_value[i];
            action = i;
    }

    assert(action != -1);
    ActionE max_action = static_cast<ActionE>(action);
    return Action {
        .action = max_action
    };
}

std::vector<double> Agent::getNRandomNumber(int n, int start, int end)
{
    std::vector<double> rand_numbers;

    uint64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::seed_seq ss{uint32_t(timeSeed & 0xffffffff), uint32_t(timeSeed>>32)};
    rng.seed(ss);

    std::uniform_real_distribution<double> unif(start, end);

    for (int i = 0; i < n; i++) {
        rand_numbers.push_back(unif(rng));
    }

    return rand_numbers;
}