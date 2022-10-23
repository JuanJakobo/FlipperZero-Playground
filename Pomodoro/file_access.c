//------------------------------------------------------------------
// file_access.c
//
// Author:           JuanJakobo
// Date:             22.10.22
//-------------------------------------------------------------------

#include "file_access.h"

const char *POMODORO_FILE_DIR_PATH = "/ext/apps/misc";
const char *POMODORO_FILE_PATH = "/ext/apps/misc/pomdoro.conf";

const char *POMODORO_FILE_HEADER = "Flipper Pomodoro plugin config file";
const int POMODORO_FILE_ACTUAL_VERSION = 1;
const char *POMODORO_CONFIG_KEY_WORK_TIME = "workTime";
const char *POMODORO_CONFIG_KEY_SHORT_BREAK_TIME = "shortBreakTime";
const char *POMODORO_CONFIG_KEY_LONG_BREAK_TIME = "longBreakTime";
const char *POMODORO_CONFIG_KEY_COUNT = "count";
const char *POMODORO_CONFIG_KEY_REPETITIONS = "repetitions";
const char *POMODORO_CONFIG_KEY_STATE = "state";

/**
 * Closes the openend file
 *
 * @param file file that shall be closed
 */
 static void pomodoro_close_config_file(FlipperFormat* file) {
    if(file == NULL) return;
    flipper_format_file_close(file);
    flipper_format_free(file);
}

/**
 * Tryies to open the config file, if it does not exists, looks for the dir, if that does not exists, creates dir, then
 * the file.
 *
 * @param storage
 */
 static FlipperFormat* pomodoro_open_config_file(Storage* storage) {
    FlipperFormat* file = flipper_format_file_alloc(storage);

    if(storage_common_stat(storage, POMODORO_FILE_PATH, NULL) == FSE_OK) {
        if(!flipper_format_file_open_existing(file, POMODORO_FILE_PATH)) {
            pomodoro_close_config_file(file);
            return NULL;
        }
    } else {
        if(storage_common_stat(storage, POMODORO_FILE_DIR_PATH, NULL) == FSE_NOT_EXIST) {
            if(!storage_simply_mkdir(storage, POMODORO_FILE_DIR_PATH)) {
                return NULL;
            }
        }

        if(!flipper_format_file_open_new(file, POMODORO_FILE_PATH)) {
            pomodoro_close_config_file(file);
            return NULL;
        }

        flipper_format_write_header_cstr(file, POMODORO_FILE_HEADER, POMODORO_FILE_ACTUAL_VERSION);
        flipper_format_rewind(file);
    }
    return file;
}


/**
 * Saves the current run to the config file so it can be initializied again
 *
 * @param pomodoro contains the run values to be saved
 */
 void pomodoro_save_current_run(const Pomodoro *pomodoro) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = pomodoro_open_config_file(storage);

    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_COUNT, &pomodoro->count, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_REPETITIONS, &pomodoro->repetitions, 1);
    uint32_t state = pomodoro->state;
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_STATE, &state, 1);

    pomodoro_close_config_file(file);
    furi_record_close(RECORD_STORAGE);
}

/**
 * Saves the times for each run
 *
 * @param pomodoro contains the times for the runs to be saved
 */
 void pomodoro_save_settings(const Pomodoro *pomodoro) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = pomodoro_open_config_file(storage);

    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_WORK_TIME, &pomodoro->workTime, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_SHORT_BREAK_TIME, &pomodoro->shortBreakTime, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_LONG_BREAK_TIME, &pomodoro->longBreakTime, 1);

    pomodoro_close_config_file(file);
    furi_record_close(RECORD_STORAGE);
}

/**
 * Loads the values from the config file, on error returns default values
 *
 * @param pomodoro variable where the loaded items are written to
 */
 void pomodoro_get_initial_values(Pomodoro *pomodoro){
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = pomodoro_open_config_file(storage);

    FuriString* file_type = furi_string_alloc();

    uint32_t version = 1;
    if(!flipper_format_read_header(file, file_type, &version)) {
        furi_string_free(file_type);
        return;
    }
    furi_string_free(file_type);

    if(!flipper_format_read_uint32(file, POMODORO_CONFIG_KEY_WORK_TIME, &pomodoro->workTime, 1)){
        pomodoro->workTime = 25;
    }

    if(!flipper_format_read_uint32(file, POMODORO_CONFIG_KEY_SHORT_BREAK_TIME, &pomodoro->shortBreakTime, 1)) {
        pomodoro->shortBreakTime = 5;
    }

    if(!flipper_format_read_uint32(file, POMODORO_CONFIG_KEY_LONG_BREAK_TIME, &pomodoro->longBreakTime, 1)) {
        pomodoro->longBreakTime = 30;
    }

    if(!flipper_format_read_uint32(file, POMODORO_CONFIG_KEY_COUNT, &pomodoro->count, 1)) {
        pomodoro->count = 0;
    }

    if(!flipper_format_read_uint32(file, POMODORO_CONFIG_KEY_REPETITIONS, &pomodoro->repetitions, 1)) {
        pomodoro->repetitions = 0;
    }

    pomodoro->running = (pomodoro->count > 0 || pomodoro->repetitions > 0) ? true : false;

    uint32_t state;
    if(!flipper_format_read_uint32(file, POMODORO_CONFIG_KEY_STATE, &state, 1)) {
        pomodoro->state = 0;
    }else{
        pomodoro->state = state;
    }

    switch(pomodoro->state){
        case longBreakTime:
            pomodoro->endTime = &pomodoro->longBreakTime;
            break;
        case shortBreakTime:
            pomodoro->endTime = &pomodoro->shortBreakTime;
            break;
        default:
            pomodoro->endTime = &pomodoro->workTime;
            break;
    }

    pomodoro_close_config_file(file);
    furi_record_close(RECORD_STORAGE);
}
