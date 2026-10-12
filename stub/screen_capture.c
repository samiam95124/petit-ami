/*******************************************************************************
*                                                                              *
* Screen capture stub for Windows                                              *
*                                                                              *
* screen_capture() is a Linux-only testing utility. This stub exists so that  *
* graphics_test can be linked on Windows without modification.                 *
*                                                                              *
*******************************************************************************/

#include <stdio.h>

void screen_capture(void) {}
void screen_capture_name(const char* fn) { (void)fn; }
void screen_capture_label(const char* s) { (void)s; }
void screen_capture_window(FILE* f) { (void)f; }
void screen_capture_end(void) {}
