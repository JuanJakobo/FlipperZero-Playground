//------------------------------------------------------------------
// file_access.h
//
// Author:           JuanJakobo
// Date:             22.10.22
// Description:      Enables to store and retrieve data stored in a config file
//-------------------------------------------------------------------

#ifndef FILE_ACCESS
#define FILE_ACCESS
#include <furi.h>

#include <flipper_format/flipper_format.h>
#include "types.h"


#define CONFIG_FILE_DIRECTORY_PATH "/ext/apps/misc"
#define CONFIG_FILE_PATH CONFIG_FILE_DIRECTORY_PATH "/pomodoro.conf"

#define CONFIG_FILE_HEADER "Flipper Pomodoro plugin config file"
#define CONFIG_FILE_ACTUAL_VERSION 1
#define POMODORO_CONFIG_KEY_WORK_TIME "workTime"
#define POMODORO_CONFIG_KEY_SHORT_BREAK_TIME "shortBreakTime"
#define POMODORO_CONFIG_KEY_LONG_BREAK_TIME "longBreakTime"
#define POMODORO_CONFIG_KEY_COUNT "count"
#define POMODORO_CONFIG_KEY_REPETITIONS "repetitions"
#define POMODORO_CONFIG_KEY_STATE "state"

// Default values
#define WORK_TIME 25
#define SHORT_BREAK_TIME 5
#define LONG_BREAK_TIME 30


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

#endif
