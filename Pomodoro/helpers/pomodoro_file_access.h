#pragma once
//------------------------------------------------------------------
// file_access.h
//
// Author:           JuanJakobo
// Date:             22.10.22
// Description:      Enables to store and retrieve data stored in a config file
//-------------------------------------------------------------------

#include <furi.h>
#include <flipper_format/flipper_format.h>
#include "pomodoro_types.h"

#define POMODORO_FILE_DIR_PATH "/ext/apps/misc"
#define POMODORO_FILE_PATH POMODORO_FILE_DIR_PATH "/pomodoro.conf"

#define POMODORO_FILE_HEADER "Flipper Pomodoro plugin config file"
#define POMODORO_FILE_ACTUAL_VERSION 1
#define POMODORO_CONFIG_KEY_WORK_TIME "workTime"
#define POMODORO_CONFIG_KEY_SHORT_BREAK_TIME "shortBreakTime"
#define POMODORO_CONFIG_KEY_LONG_BREAK_TIME "longBreakTime"
#define POMODORO_CONFIG_KEY_COUNT "count"
#define POMODORO_CONFIG_KEY_REPETITIONS "repetitions"
#define POMODORO_CONFIG_KEY_STATE "state"
#define POMODORO_CONFIG_KEY_TOTAL_RUNS "totalRuns"
/**
 * @param pomodoro Pomodoro object to be saved
 */
 void pomodoro_save_current_run(const Pomodoro *pomodoro);

/**
 * @param pomodoro Pomodoro object to be saved
 */
 void pomodoro_save_settings(const Pomodoro *pomodoro);

/**
 * @param pomodoro Pomodoro object that should store the values
 */
 void pomodoro_get_initial_values(Pomodoro *pomodoro);
