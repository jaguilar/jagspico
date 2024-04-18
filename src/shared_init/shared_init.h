#ifndef SHARED_INIT_H
#define SHARED_INIT_H

// Code using the shared_init main code will implement the following task
// handler to mark the "entry point" of their programs.
extern
#ifdef __cplusplus
    "C"
#endif
    void
    main_task(void* arg);

#endif