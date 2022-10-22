//------------------------------------------------------------------
// types.h
//
// Author:           JuanJakobo
// Date:             22.10.22
// Description:      Stores the custom types used in different files
//-------------------------------------------------------------------

#ifndef TYPES
#define TYPES

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

#endif
