#ifndef __ASUS_FG_H
#define __ASUS_FG_H

#include "linux/asusdebug.h"

#define BATTERY_TAG "[BAT][BMS]"
#define ERROR_TAG "[ERR]"
#define BAT_DBG(...)  printk(KERN_INFO BATTERY_TAG __VA_ARGS__)
#define BAT_DBG_L(level, ...)  printk(level BATTERY_TAG __VA_ARGS__)
#define BAT_DBG_E(...)  printk(KERN_ERR BATTERY_TAG ERROR_TAG __VA_ARGS__)

#define FAKE_TEMP_INIT	180

int asus_fg_porting(struct fg_chip *chip);
void asus_set_battery_version(void);
void asus_check_batt_id(struct fg_chip *chip);

extern struct fg_chip *g_fgChip;
extern bool g_charger_mode;
extern int fake_temp;

#endif
