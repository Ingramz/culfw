#ifndef _kopp_fc_H
#define _kopp_fc_H
#define MAX_kopp_fc_MSG 11
extern uint8_t kopp_fc_on;
extern uint8_t blkctr; 
// #define SOMFY_RTS_FRAME_SIZE 7

/* public kopp function call */
void kopp_fc_init(void);
// void kopp_fc_task(void);
void kopp_fc_func(char *in);

#endif
