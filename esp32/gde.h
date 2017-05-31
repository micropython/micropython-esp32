#ifndef EPD_GDE_H
#define EPD_GDE_H

#include <stdbool.h>
#include <stdint.h>

extern void gdeInit(void);
extern void gdeReset(void);
extern bool gdeIsBusy(void);
extern void gdeBusyWait(void);
extern void gdeWriteByte(uint8_t data);
extern void gdeWriteCommand(uint8_t command);
extern void gdeWriteCommandInit(uint8_t command);
extern void gdeWriteCommandEnd(void);

static inline void gdeWriteCommand_p1(uint8_t command, uint8_t para1) {
	gdeWriteCommandInit(command);
	gdeWriteByte(para1);
	gdeWriteCommandEnd();
}

static inline void gdeWriteCommand_p2(uint8_t command, uint8_t para1,
                                      uint8_t para2) {
	gdeWriteCommandInit(command);
	gdeWriteByte(para1);
	gdeWriteByte(para2);
	gdeWriteCommandEnd();
}

static inline void gdeWriteCommand_p3(uint8_t command, uint8_t para1,
                                      uint8_t para2, uint8_t para3) {
	gdeWriteCommandInit(command);
	gdeWriteByte(para1);
	gdeWriteByte(para2);
	gdeWriteByte(para3);
	gdeWriteCommandEnd();
}

static inline void gdeWriteCommand_p4(uint8_t command, uint8_t para1,
                                      uint8_t para2, uint8_t para3,
                                      uint8_t para4) {
	gdeWriteCommandInit(command);
	gdeWriteByte(para1);
	gdeWriteByte(para2);
	gdeWriteByte(para3);
	gdeWriteByte(para4);
	gdeWriteCommandEnd();
}

static inline void gdeWriteCommandStream(uint8_t command, const uint8_t *data,
                                         unsigned int datalen) {
	gdeWriteCommandInit(command);
	while (datalen-- > 0) {
		gdeWriteByte(*(data++));
	}
	gdeWriteCommandEnd();
}

#endif
