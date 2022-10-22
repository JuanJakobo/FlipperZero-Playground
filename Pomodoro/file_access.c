//------------------------------------------------------------------
// file_access.c
//
// Author:           JuanJakobo
// Date:             22.10.22
//-------------------------------------------------------------------

#include "file_access.h"

/**
 * opens the storage
 */
 static Storage* pomodoro_open_storage() {
    return furi_record_open(RECORD_STORAGE);
}

/**
 * closes the storage
 */
static void pomodoro_close_storage() {
    furi_record_close(RECORD_STORAGE);
}

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
 * the file. The file is filled with default values on creation.
 *
 * @param storage
 */
 static FlipperFormat* pomodoro_open_config_file(Storage* storage) {
    FlipperFormat* format = flipper_format_file_alloc(storage);

    if(storage_common_stat(storage, CONFIG_FILE_PATH, NULL) == FSE_OK) {
        if(!flipper_format_file_open_existing(format, CONFIG_FILE_PATH)) {
            pomodoro_close_config_file(format);
            return NULL;
        }
    } else {
        if(storage_common_stat(storage, CONFIG_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
            if(!storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH)) {
                return NULL;
            }
        }

        if(!flipper_format_file_open_new(format, CONFIG_FILE_PATH)) {
            pomodoro_close_config_file(format);
            return NULL;
        }

        //Default Values
        flipper_format_write_header_cstr(format, CONFIG_FILE_HEADER, CONFIG_FILE_ACTUAL_VERSION);
        flipper_format_write_comment_cstr(format, " ");

        const uint32_t workTime = WORK_TIME;
        flipper_format_write_comment_cstr(format, "Time of a Pomodoro");
        flipper_format_write_uint32(format, POMODORO_CONFIG_KEY_WORK_TIME, &workTime, 1);

        uint32_t shortBreakTime = SHORT_BREAK_TIME;
        flipper_format_write_comment_cstr(format, "Time of a short Break");
        flipper_format_write_uint32(format, POMODORO_CONFIG_KEY_SHORT_BREAK_TIME, &shortBreakTime, 1);

        uint32_t longBreakTime = LONG_BREAK_TIME;
        flipper_format_write_comment_cstr( format, "Time of a long Break");
        flipper_format_write_uint32(format, POMODORO_CONFIG_KEY_LONG_BREAK_TIME, &longBreakTime, 1);

        uint32_t defaultValue = 0;
        flipper_format_write_comment_cstr(format, " ");
        flipper_format_write_comment_cstr(format, "Values for current run.");
        flipper_format_write_comment_cstr( format, "Minutes in current state");
        flipper_format_write_uint32(format, POMODORO_CONFIG_KEY_COUNT, &defaultValue, 1);
        flipper_format_write_comment_cstr( format, "Repititions in current run");
        flipper_format_write_uint32(format, POMODORO_CONFIG_KEY_REPETITIONS, &defaultValue, 1);
        flipper_format_write_comment_cstr( format, "Current state(0 = work, 1 = short Break, 2 = long Break");
        flipper_format_write_uint32(format, POMODORO_CONFIG_KEY_STATE, &defaultValue, 1);

    }

    if(!flipper_format_rewind(format)) {
        pomodoro_close_config_file(format);
        return NULL;
    }

    return format;
}


/**
 * Saves the current run to the config file so it can be initializied again
 *
 * @param pomodoro contains the run values to be saved
 */
 void pomodoro_save_current_run(const Pomodoro *pomodoro) {
    Storage* cfg_storage = pomodoro_open_storage();
    FlipperFormat* file = pomodoro_open_config_file(cfg_storage);

    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_COUNT, &pomodoro->count, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_REPETITIONS, &pomodoro->repetitions, 1);
    uint32_t state = pomodoro->state;
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_STATE, &state, 1);

    pomodoro_close_config_file(file);
    pomodoro_close_storage();
}

/**
 * Saves the times for each run
 *
 * @param pomodoro contains the times for the runs to be saved
 */
 void pomodoro_save_settings(const Pomodoro *pomodoro) {
    Storage* cfg_storage = pomodoro_open_storage();
    FlipperFormat* file = pomodoro_open_config_file(cfg_storage);

    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_WORK_TIME, &pomodoro->workTime, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_SHORT_BREAK_TIME, &pomodoro->shortBreakTime, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_LONG_BREAK_TIME, &pomodoro->longBreakTime, 1);

    pomodoro_close_config_file(file);
    pomodoro_close_storage();
}

/**
 * Loads the values from the config file, on error returns default values
 *
 * @param pomodoro variable where the loaded items are written to
 */
 void pomodoro_get_initial_values(Pomodoro *pomodoro){
    Storage* storage = pomodoro_open_storage();
    FlipperFormat* format = pomodoro_open_config_file(storage);

    FuriString* file_type = furi_string_alloc();

    //TODO use other int
    uint32_t version = 1;
    if(!flipper_format_read_header(format, file_type, &version)) {
        furi_string_free(file_type);
        return;
    }

    flipper_format_rewind(format);

    //TODO set default values once
    if(!flipper_format_read_uint32( format, POMODORO_CONFIG_KEY_WORK_TIME, &pomodoro->workTime, 1)){
        pomodoro->workTime = WORK_TIME;
    }

    if(!flipper_format_read_uint32( format, POMODORO_CONFIG_KEY_SHORT_BREAK_TIME, &pomodoro->shortBreakTime, 1)) {
        pomodoro->shortBreakTime = SHORT_BREAK_TIME;
    }

    if(!flipper_format_read_uint32( format, POMODORO_CONFIG_KEY_LONG_BREAK_TIME, &pomodoro->longBreakTime, 1)) {
        pomodoro->longBreakTime = LONG_BREAK_TIME;
    }

    if(!flipper_format_read_uint32( format, POMODORO_CONFIG_KEY_COUNT, &pomodoro->count, 1)) {
        pomodoro->count = 0;
    }

    if(!flipper_format_read_uint32( format, POMODORO_CONFIG_KEY_REPETITIONS, &pomodoro->repetitions, 1)) {
        pomodoro->repetitions = 0;
    }

    pomodoro->running = (pomodoro->count > 0 || pomodoro->repetitions > 0) ? true : false;

    uint32_t state;
    if(!flipper_format_read_uint32( format, POMODORO_CONFIG_KEY_STATE, &state, 1)) {
        pomodoro->state = 0;
    }

    //TODO state?
    pomodoro->state = state;
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

    furi_string_free(file_type);
    pomodoro_close_config_file(format);
    pomodoro_close_storage();
}
