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
