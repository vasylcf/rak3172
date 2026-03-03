/**
 * @file    app_config.h
 * @brief   Build-time switch: compile as MASTER or SLAVE
 *
 * Set exactly one of the two defines to 1.
 * Alternatively pass -DAPP_ROLE_MASTER=1 in the Keil C/C++ preprocessor.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ---- Choose role (set ONE to 1) ---- */
#define APP_ROLE_MASTER   1
#define APP_ROLE_SLAVE    0

/* ---- Slave serial-number filter ---- */
#define SLAVE_SERIAL_NUMBER   "508011"
#define SLAVE_SERIAL_LEN      6

/* ---- Master fixed payload ---------- */
#define MASTER_PAYLOAD        "508012"

#endif /* APP_CONFIG_H */
