// AI-ASSISTED
// Matt Krueger
// April 2026

#pragma once

#ifndef POOLANTIR_NODE_ID
#define POOLANTIR_NODE_ID "0"
#endif

#define PIN_SERVO 14
#define LED_R     25
#define LED_G     26
#define LED_B     27
#define TOF_SDA   21
#define TOF_SCL   22

#define DEFAULT_IN_RANGE_MM    60
#define DEFAULT_SERVO_RAMP_MS  2000

#define SERVO_REST_DEG  0
#define SERVO_MAX_DEG   180

#define LED_FLASH_MS    1000
#define SIM_GAP_MS      300

enum Mode { MODE_TEST, MODE_SIM };
