#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

typedef struct {
    uint8_t x;
    uint8_t y;
} Point;

typedef struct {
    Point loc;
    uint8_t move_x;
    uint8_t change_r;
    int r; //radius of the circle
} Circle;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} CircleEvent;

// Screen is 128x64 px
const int MAX_X = 128;
const int MAX_Y = 64;
const int BORDER = 2;
const int MENU_BEGIN_Y = MAX_Y - 10;

//draw a random dot
//draw stuff to the screen
void draw_callback(Canvas* const canvas, void* ctx) {
    const Circle* circle = acquire_mutex((ValueMutex*)ctx, 25);
    if(circle == NULL) {
        return;
    }
    //draw menu bar
    canvas_draw_frame(canvas, 0, MENU_BEGIN_Y, MAX_X, MAX_Y);
    char str[35];
    snprintf(str,sizeof str, "Rad: %d, Pos x: %d, Pos y: %d", circle->r, circle->loc.x, circle->loc.y);
    canvas_draw_str(canvas, 2,MAX_Y,str);

    //draw the circle
    canvas_draw_dot(canvas, circle->loc.x,circle->loc.y);
    canvas_draw_circle(canvas, circle->loc.x, circle->loc.y, circle->r);

    release_mutex((ValueMutex*)ctx, circle);
}

void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    CircleEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void circle_init(Circle* const circle) {
    circle->r = 5;
    circle->loc.x = rand() % (MAX_X-circle->r);
    circle->loc.y = MENU_BEGIN_Y/2;
    circle->move_x = 2;
    circle->change_r = 1;
}

int32_t circle_app(void* p) {
    UNUSED(p);

    Circle* circle = malloc(sizeof(Circle));
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(CircleEvent));
    circle_init(circle);

    ValueMutex state_mutex;
    if(!init_mutex(&state_mutex, circle, sizeof(Circle))) {
        FURI_LOG_E("Circle", "cannot create mutex\r\n");
        free(circle);
        return 255;
    }
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &state_mutex);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    CircleEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        Circle* circle = (Circle*)acquire_mutex_block(&state_mutex);

        if(event_status == FuriStatusOk) {
            // key events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                        case InputKeyUp:
                                circle->r -= 1;
                            break;
                        case InputKeyDown:
                            if(circle->r < MENU_BEGIN_Y / 2)
                                circle->r += 1;
                            break;
                        case InputKeyRight:
                            if(((circle->loc.x + circle->move_x + circle->r) < MAX_X ))
                                    circle->loc.x += circle->move_x;
                            break;
                        case InputKeyLeft:
                            if((circle->loc.x + circle->move_x - circle->r) > 0)
                                circle->loc.x -= circle->move_x;
                            break;
                        case InputKeyBack:
                            processing = false;
                            break;
                        case InputKeyOk:
                            break;
                    }
                }
            }
        }

        view_port_update(view_port);
        release_mutex(&state_mutex, circle);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    delete_mutex(&state_mutex);
    free(circle);

    return 0;
}
