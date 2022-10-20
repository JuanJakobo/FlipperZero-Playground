#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

//Decide on the task to be done.
//Set the pomodoro timer (typically for 25 minutes).[1]
//Work on the task.
//End work when the timer rings and take a short break (typically 5–10 minutes).[5]
//If you have finished fewer than three pomodoros, go back to Step 2 and repeat until you go through all three pomodoros.
//After three pomodoros are done, take the fourth pomodoro and then take a long break (typically 20 to 30 minutes). Once the long break is finished, return to step 2.
//https://en.wikipedia.org/wiki/Pomodoro_Technique


//TODO check memory
//uint8_t x;
typedef struct {
    int count;
    int *endTime;
    int workTime;
    int shortBreakTime;
    int longBreakTime;
    int repetitions;
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
    //&message_display_backlight_off,
    //&message_blink_set_color_magenta,
    &message_blue_255,
    &message_vibro_on,
    &message_blink_start_10,
    &message_delay_100,
    &message_vibro_off,
    //&message_blink_stop,
    &message_blue_0,
    //&message_display_backlight_on,
    NULL,
};

void draw_callback(Canvas* const canvas, void* ctx) {
    const Pomodoro* pomodoro = acquire_mutex((ValueMutex*)ctx, 25);
    if(pomodoro == NULL) {
        return;
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 17, 20, 92, 24);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 17, 20, 92, 24);

    canvas_set_font(canvas, FontPrimary);
    char buffer[24];
    //TODO test if always
    if(!pomodoro->running){
        canvas_draw_str(canvas, 37, 31, "OK to Start");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignBottom, "< Change value > ");
        canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, "^ Change time ");
    }else{
        canvas_draw_str(canvas, 37, 31, "Ok to Stop");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buffer,sizeof buffer, "Reps: %d, Timer %ds", pomodoro->repetitions, pomodoro->count);
        canvas_draw_str_aligned(canvas, 64, 64, AlignCenter, AlignBottom, buffer);
    }

    //TODO shorten
    if(pomodoro->endTime == &pomodoro->shortBreakTime)
        snprintf(buffer, sizeof(buffer), " %s(%ds) ", "Short break", *pomodoro->endTime);
    else if(pomodoro->endTime == &pomodoro->longBreakTime)
        snprintf(buffer, sizeof(buffer), " %s(%ds) ", "Long break", *pomodoro->endTime);
    else
        snprintf(buffer, sizeof(buffer), " %s(%ds) ", "Work", *pomodoro->endTime);
    canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignBottom, buffer);
    release_mutex((ValueMutex*)ctx, pomodoro);
}

void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PomodoroEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void
    pomodoro_process_game_step(Pomodoro* const pomodoro, NotificationApp* notification) {
        pomodoro->count++;
        if((pomodoro->count) >= *pomodoro->endTime){
            notification_message(notification, &time_up);
            //notification_message_block(notification, &time_up);
            if(!pomodoro->notification){
                pomodoro->notification = true;
            }
        }
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
        pomodoro->endTime = &pomodoro->shortBreakTime;
        pomodoro->repetitions++;
        if(pomodoro->repetitions == 4){
            pomodoro->repetitions = 0;
            pomodoro->endTime = &pomodoro->longBreakTime;
        }
    }else{
        pomodoro->endTime = &pomodoro->workTime;
    }
}

static void pomodoro_init(Pomodoro* const pomodoro) {
    //TODO change to minutes
    pomodoro->count = 0;
    pomodoro->workTime = 1500;
    pomodoro->shortBreakTime = 300;
    pomodoro->longBreakTime = 1800;
    pomodoro->endTime= &pomodoro->workTime;
    pomodoro->repetitions = 0;
    pomodoro->running = false;
}

int32_t pomodoro_app(void* p) {
    UNUSED(p);

    Pomodoro* pomodoro = malloc(sizeof(Pomodoro));
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PomodoroEvent));
    pomodoro_init(pomodoro);

    ValueMutex state_mutex;
    if(!init_mutex(&state_mutex, pomodoro, sizeof(Pomodoro))) {
        FURI_LOG_E("Pomodoro", "cannot create mutex\r\n");
        free(pomodoro);
        return 255;
    }
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &state_mutex);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    FuriTimer* timer = furi_timer_alloc(pomodoro_update_timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_ms_to_ticks(1000));

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
                                if(!pomodoro->running && (*pomodoro->endTime -1) >= 0)
                                    (*pomodoro->endTime)--;
                                else if(pomodoro->notification)
                                    pomodoro_stop_notification(pomodoro);
                            break;
                        //Increase timer for choosen object, if start false
                        case InputKeyRight:
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
                            break;
                        //Close App
                        case InputKeyBack:
                            processing = false;
                            break;
                        //Pause Pomodoro
                        case InputKeyOk:
                            pomodoro->running = !pomodoro->running;
                            if(pomodoro->running){
                                pomodoro->endTime = &pomodoro->workTime;
                                pomodoro->count = 0;
                                pomodoro->repetitions = 0;
                            }
                            break;
                    }
                }
            } else if(event.type == EventTypeTick) {
                if(pomodoro->running)
                    pomodoro_process_game_step(pomodoro, notification);
            }
        }

        view_port_update(view_port);
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
