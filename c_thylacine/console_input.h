#ifndef _CONSOLE_INPUT_H_
#define _CONSOLE_INPUT_H_

void console_input_init(const char *);
void console_input_fini(void);
char *console_input(const char *);
void console_input_history_add(const char *);


#endif
