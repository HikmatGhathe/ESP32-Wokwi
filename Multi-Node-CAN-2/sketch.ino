#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h" 
#include "driver/gpio.h"

#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4
#define IDENTITY_PIN GPIO_NUM_12

bool is_engine_node = false;

void setup() {
    Serial.begin(115200);
    
    // 1. Read the Hardware Identity Pin
    gpio_reset_pin(IDENTITY_PIN);
    gpio_set_direction(IDENTITY_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(IDENTITY_PIN, GPIO_PULLUP_ONLY);
    delay(100); 

    if (gpio_get_level(IDENTITY_PIN) == 0) {
        is_engine_node = true;
        Serial.println("\n--- Booting Node A (Vehicle Spammer) ---");
    } else {
        is_engine_node = false;
        Serial.println("\n--- Booting Node B (Dashboard Unit) ---");
    }

    // 2. Configure the CAN Hardware
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config;

    if (is_engine_node) {
        // Node A sends everything, so it doesn't need a filter.
        f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    } else {
        // Node B Hardware Filter: Reject everything except 0x036
        f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        f_config.acceptance_code = (0x036 << 21);
        f_config.acceptance_mask = ~(0x7FF << 21);
        f_config.single_filter = true;
    }

    // 3. Start the Network
    twai_driver_install(&g_config, &t_config, &f_config);
    twai_start();
}

void loop() {
    if (is_engine_node) {
        // --------------------------------------------------
        // VEHICLE LOGIC: Flood the network with 3 different IDs
        // --------------------------------------------------
        twai_message_t msg;
        msg.extd = 0;             
        msg.rtr = 0;              
        
        // Message 1: Brakes (ID 0x010)
        msg.identifier = 0x010;   
        msg.data_length_code = 1; 
        msg.data[0] = 0xFF; // Brakes engaged
        twai_transmit(&msg, pdMS_TO_TICKS(100));

        // Message 2: Engine RPM (ID 0x036)
        msg.identifier = 0x036;   
        msg.data_length_code = 2; 
        msg.data[0] = 0x0D; // 3500 RPM
        msg.data[1] = 0xAC;
        twai_transmit(&msg, pdMS_TO_TICKS(100));

        // Message 3: Radio Volume (ID 0x120)
        msg.identifier = 0x120;   
        msg.data_length_code = 1; 
        msg.data[0] = 0x05; // Volume level 5
        twai_transmit(&msg, pdMS_TO_TICKS(100));

        Serial.println("[NODE A] Flooded network with IDs: 0x010, 0x036, 0x120");
        delay(1000); 
    } 
    else {
        // --------------------------------------------------
        // DASHBOARD LOGIC: Prove the hardware filter works
        // --------------------------------------------------
        twai_message_t received_msg;
        
        // CPU sleeps here. The hardware filter protects it from 0x010 and 0x120.
        if (twai_receive(&received_msg, pdMS_TO_TICKS(100)) == ESP_OK) {
            
            if (received_msg.identifier == 0x036) {
                int rpm = (received_msg.data[0] << 8) | received_msg.data[1];
                Serial.print("[NODE B] Filter Passed! Engine RPM: ");
                Serial.println(rpm);
            } else {
                // If this prints, your hardware filter failed!
                Serial.println("[NODE B] ERROR: Caught a message I shouldn't have!");
            }
        }
    }
}