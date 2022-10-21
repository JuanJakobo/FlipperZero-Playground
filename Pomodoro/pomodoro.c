#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <flipper_format/flipper_format.h>

//Decide on the task to be done.
//Set the pomodoro timer (typically for 25 minutes).[1]
//Work on the task.
//End work when the timer rings and take a short break (typically 5–10 minutes).[5]
//If you have finished fewer than three pomodoros, go back to Step 2 and repeat until you go through all three pomodoros.
//After three pomodoros are done, take the fourth pomodoro and then take a long break (typically 20 to 30 minutes). Once the long break is finished, return to step 2.
//https://en.wikipedia.org/wiki/Pomodoro_Technique

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

typedef enum {
    workTime = 0,
    shortBreakTime = 1,
    longBreakTime = 2,
} PomodoroState;

//TODO use other int?
typedef struct {
    uint32_t count;
    uint32_t *endTime;
    uint32_t workTime;
    uint32_t shortBreakTime;
    uint32_t longBreakTime;
    uint32_t repetitions;
    PomodoroState state;
    bool running;
    bool notification;
} Pomodoro;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PomodoroEvent;

const NotificationSequence time_up = {
    &message_vibro_on,
    &message_blue_255,

    &message_note_ds4,
    &message_delay_100,
    &message_sound_off,

    &message_blue_0,
    &message_vibro_off,
    NULL,
};

void draw_callback(Canvas* const canvas, void* ctx) {
    const Pomodoro* pomodoro = acquire_mutex((ValueMutex*)ctx, 25);
    if(pomodoro == NULL) {
        return;
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 14, 20, 100, 24);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 14, 20, 100, 24);

    char buffer[30];
    canvas_set_font(canvas, FontPrimary);
    //TODO shorten
    if(pomodoro->endTime == &pomodoro->shortBreakTime)
        snprintf(buffer, sizeof(buffer), " %s(%ld min) ", "Short break", *pomodoro->endTime);
    else if(pomodoro->endTime == &pomodoro->longBreakTime)
        snprintf(buffer, sizeof(buffer), " %s(%ld min) ", "Long break", *pomodoro->endTime);
    else
        snprintf(buffer, sizeof(buffer), " %s(%ld min) ", "Work", *pomodoro->endTime);

    canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignBottom, buffer);

    if(pomodoro->running){
        canvas_set_font(canvas, FontSecondary);
        snprintf(buffer,sizeof buffer, "Reps %ld, Timer %ld min", pomodoro->repetitions, pomodoro->count);
        canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignBottom, buffer);
        canvas_draw_str(canvas, 2, 64, "OK to Pause, Back to restart");
    }else{
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignBottom, "< Change value > ");
        canvas_draw_str(canvas, 2, 64, "OK to start, Back to close");
    }

    if(pomodoro->notification)
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "Time is up, press arrow key");

    release_mutex((ValueMutex*)ctx, pomodoro);
}

void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PomodoroEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void pomodoro_update_timer_callback(FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PomodoroEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static void pomodoro_stop_notification(Pomodoro *pomodoro){
    pomodoro->notification = false;
    pomodoro->count = 0;
    if(pomodoro->endTime == &pomodoro->workTime){
        pomodoro->repetitions++;
        if(pomodoro->repetitions == 4){
            pomodoro->repetitions = 0;
            pomodoro->state = longBreakTime;
            pomodoro->endTime = &pomodoro->longBreakTime;
        }else{
            pomodoro->state = shortBreakTime;
            pomodoro->endTime = &pomodoro->shortBreakTime;
        }
    }else{
        pomodoro->state = workTime;
        pomodoro->endTime = &pomodoro->workTime;
    }
}

//File handling
static Storage* pomodoro_open_storage() {
    return furi_record_open(RECORD_STORAGE);
}

static void pomodoro_close_storage() {
    furi_record_close(RECORD_STORAGE);
}

static void pomodoro_close_config_file(FlipperFormat* file) {
    if(file == NULL) return;
    flipper_format_file_close(file);
    flipper_format_free(file);
}

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


static void pomodoro_save_current_run(Pomodoro * const pomodoro) {
    Storage* cfg_storage = pomodoro_open_storage();
    FlipperFormat* file = pomodoro_open_config_file(cfg_storage);

    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_COUNT, &pomodoro->count, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_REPETITIONS, &pomodoro->repetitions, 1);
    uint32_t state = pomodoro->state;
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_STATE, &state, 1);

    pomodoro_close_config_file(file);
    pomodoro_close_storage();
}

static void pomodoro_save_settings(Pomodoro * const pomodoro) {
    Storage* cfg_storage = pomodoro_open_storage();
    FlipperFormat* file = pomodoro_open_config_file(cfg_storage);

    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_WORK_TIME, &pomodoro->workTime, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_SHORT_BREAK_TIME, &pomodoro->shortBreakTime, 1);
    flipper_format_insert_or_update_uint32(file, POMODORO_CONFIG_KEY_LONG_BREAK_TIME, &pomodoro->longBreakTime, 1);

    pomodoro_close_config_file(file);
    pomodoro_close_storage();
}


static void pomodoro_init(Pomodoro* const pomodoro) {

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

int32_t pomodoro_app(void* p) {
    UNUSED(p);

    Pomodoro* pomodoro = malloc(sizeof(Pomodoro));
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PomodoroEvent));

    ValueMutex state_mutex;
    if(!init_mutex(&state_mutex, pomodoro, sizeof(Pomodoro))) {
        FURI_LOG_E("Pomodoro", "cannot create mutex\r\n");
        free(pomodoro);
        return 255;
    }

    pomodoro_init(pomodoro);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &state_mutex);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    //start timer when is running is true?
    FuriTimer* timer = furi_timer_alloc(pomodoro_update_timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_ms_to_ticks(1000));
    uint8_t tick = 0;

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    //notification_message_block(notification, &sequence_display_backlight_enforce_on);
    PomodoroEvent event;
    for(bool processing = true; processing;) {

        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        Pomodoro* pomodoro = (Pomodoro*)acquire_mutex_block(&state_mutex);

        if(event_status == FuriStatusOk) {
            // key events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                        //TODO shorten
                        //Select previous choosen object, if start false
                        case InputKeyUp:
                            if(!pomodoro->running)
                                (*pomodoro->endTime)++;
                            else if(pomodoro->notification)
                                pomodoro_stop_notification(pomodoro);
                            break;
                            //Select next choosen object, if start false
                        case InputKeyDown:
                            if(!pomodoro->running && (*pomodoro->endTime -1) > 0)
                                (*pomodoro->endTime)--;
                            else if(pomodoro->notification)
                                pomodoro_stop_notification(pomodoro);
                            break;
                            //Increase timer for choosen object, if start false
                        case InputKeyRight:
                            //TODO use -- and ++?
                            if(!pomodoro->running){
                                if(pomodoro->endTime == &pomodoro->workTime)
                                    pomodoro->endTime = &pomodoro->shortBreakTime;
                                else if(pomodoro->endTime == &pomodoro->shortBreakTime)
                                    pomodoro->endTime = &pomodoro->longBreakTime;
                                else
                                    pomodoro->endTime = &pomodoro->workTime;
                            }else if(pomodoro->notification)
                                pomodoro_stop_notification(pomodoro);
                            break;
                            //Decrease timer for choosen object, if start false
                        case InputKeyLeft:
                            if(!pomodoro->running){
                                if(pomodoro->endTime == &pomodoro->workTime)
                                    pomodoro->endTime = &pomodoro->longBreakTime;
                                else if(pomodoro->endTime == &pomodoro->shortBreakTime)
                                    pomodoro->endTime = &pomodoro->workTime;
                                else
                                    pomodoro->endTime = &pomodoro->shortBreakTime;
                            }else if(pomodoro->notification)
                                pomodoro_stop_notification(pomodoro);
                            break;
                            //Reset while running or Close App
                        case InputKeyBack:
                            if(pomodoro->running){
                                pomodoro->endTime = &pomodoro->workTime;
                                pomodoro->state = workTime;
                                pomodoro->count = 0;
                                pomodoro->repetitions = 0;
                                tick = 0;
                            }else
                                processing = false;
                            break;
                            //Switch between start running, save each time
                        case InputKeyOk:
                            if(!pomodoro->running){
                                pomodoro_save_settings(pomodoro);
                                switch(pomodoro->state){
                                    case shortBreakTime:
                                        pomodoro->endTime = &pomodoro->shortBreakTime;
                                        break;
                                    case longBreakTime:
                                        pomodoro->endTime = &pomodoro->longBreakTime;
                                        break;
                                    default:
                                        pomodoro->endTime = &pomodoro->workTime;
                                        break;
                                }
                            }else{
                                pomodoro_save_current_run(pomodoro);
                                pomodoro->notification = false;
                            }
                            pomodoro->running = !pomodoro->running;
                            break;
                    }
                    view_port_update(view_port);
                }
            } else if(event.type == EventTypeTick) {
                if(pomodoro->running){
                    if(++tick == 60){
                        pomodoro->count++;
                        tick = 0;
                    }
                    if(!pomodoro->notification && (pomodoro->count) >= *pomodoro->endTime){
                        pomodoro->notification = true;
                    }
                    view_port_update(view_port);
                    if(pomodoro->notification)
                        notification_message(notification, &time_up);
                }
            }
        }

        release_mutex(&state_mutex, pomodoro);
    }

    // Wait for all notifications to be played and return backlight to normal state
    //notification_message_block(notification, &sequence_display_backlight_enforce_auto);

    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    delete_mutex(&state_mutex);
    free(pomodoro);

    return 0;
}
