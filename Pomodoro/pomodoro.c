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
//TODO make pair?
typedef enum {
    Work,
    ShortBreak,
    LongBreak,
} State;

typedef struct {
    int count;
    //TODO long
    int *endTime;
    int workTime;
    int shortBreakTime;
    int longBreakTime;
    int repetitions;
    State state;
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

const NotificationSequence sequence_eat = {
    //&message_display_backlight_off,
    &message_vibro_on,
    //TODO set delay time
    &message_delay_10,
    &message_vibro_off,
    //&message_display_backlight_on,
    NULL,
};

// Screen is 128x64 px
const int MAX_X = 128;
const int MAX_Y = 64;
const int BORDER = 2;
const int MENU_BEGIN_Y = MAX_Y - 10;

void draw_callback(Canvas* const canvas, void* ctx) {
    const Pomodoro* pomodoro = acquire_mutex((ValueMutex*)ctx, 25);
    if(pomodoro == NULL) {
        return;
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 34, 20, 62, 24);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 34, 20, 62, 24);

    canvas_set_font(canvas, FontPrimary);

    if(!pomodoro->running){
        canvas_draw_str(canvas, 37, 31, "OK to Start");
    }else{
        canvas_draw_str(canvas, 37, 31, "Ok to Pause");

        char str[35];
        canvas_set_font(canvas, FontSecondary);
        snprintf(str,sizeof str, "Timer: %d s, Reps %d", pomodoro->count, pomodoro->repetitions);
        char buffer[16];
        switch(pomodoro->state){
            case Work:
                snprintf(buffer, sizeof(buffer), "current %d: %d", pomodoro->state, pomodoro->workTime);
                break;
            case ShortBreak:
                snprintf(buffer, sizeof(buffer), "current %d: %d", pomodoro->state, pomodoro->shortBreakTime);
                break;
            case LongBreak:
                snprintf(buffer, sizeof(buffer), "current %d: %d", pomodoro->state, pomodoro->longBreakTime);
                break;
        }
        canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignBottom, buffer);
        canvas_draw_str(canvas, 2,MAX_Y,str);
    }
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
        if(pomodoro->count >= *pomodoro->endTime){
            notification_message(notification, &sequence_eat);
            //notification_message_block(notification, &sequence_eat);
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

static void pomodoro_init(Pomodoro* const pomodoro) {
    pomodoro->count = 0;
    pomodoro->workTime = 5;
    pomodoro->shortBreakTime = 20;
    pomodoro->longBreakTime = 20;
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
    //TODO changes the frequency of all, use two timer?
    //furi_timer_start(timer, furi_kernel_get_tick_frequency() / 4);
    furi_timer_start(timer, furi_ms_to_ticks(1000));
    //furi_timer_start(timer, furi_ms_to_ticks(60000));
    //FuriStatus furi_delay_until_tick(uint32_t tick);

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
                        //Select previous choosen object, if start false
                        case InputKeyUp:
                                if(!pomodoro->running)
                                    pomodoro->workTime++;
                            break;
                        //Select next choosen object, if start false
                        case InputKeyDown:
                                if(!pomodoro->running)
                                    pomodoro->workTime--;
                            break;
                        //Increase timer for choosen object, if start false
                        case InputKeyRight:
                            //TODO function
                                if(!pomodoro->running)
                                    pomodoro->state++;
                                else
                                {
                                    pomodoro->notification = false;
                                    pomodoro->count = 0;
                                    //take a short break
                                    if(pomodoro->state == Work){
                                        ////TODO save to fiels?
                                        //TODO need both?
                                        pomodoro->state = ShortBreak;
                                        pomodoro->endTime = &pomodoro->shortBreakTime;
                                        pomodoro->repetitions++;
                                        if(pomodoro->repetitions == 4){
                                            pomodoro->repetitions = 0;
                                            pomodoro->state = LongBreak;
                                            pomodoro->endTime = &pomodoro->longBreakTime;
                                            //take a long break
                                        }
                                    }else{
                                        pomodoro->state = Work;
                                        pomodoro->endTime = &pomodoro->workTime;
                                    }
                                }
                                break;
                        //Decrease timer for choosen object, if start false
                        case InputKeyLeft:
                                if(!pomodoro->running && (pomodoro->workTime - 1 >= 0))
                                    pomodoro->state--;
                            break;
                        //Close App
                        case InputKeyBack:
                            if(pomodoro->running){
                                pomodoro->count = 0;
                                pomodoro->state = Work;
                                pomodoro->repetitions = 0;
                            }else{
                                processing = false;
                            }
                            break;
                        //Pause Pomodoro
                        case InputKeyOk:
                            pomodoro->running = !pomodoro->running;
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
