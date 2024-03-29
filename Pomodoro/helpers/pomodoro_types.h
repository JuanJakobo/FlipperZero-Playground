#pragma once
//------------------------------------------------------------------
// types.h
//
// Author:           JuanJakobo
// Date:             22.10.22
// Description:      Stores the custom types used in different files
//-------------------------------------------------------------------

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
    uint32_t totalruns;
    PomodoroState state;
    bool running;
    bool notification;
    FuriMutex* mutex;
} Pomodoro;
