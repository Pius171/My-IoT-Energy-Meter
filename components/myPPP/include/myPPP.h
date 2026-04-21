#ifndef MY_PPP_H
#define MY_PPP_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "esp_event.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the underlying system components:
 * Netif, Default Event Loop, and Event Handlers.
 */
void ppp_setup(void);



/**
    * @brief Configures the PPP network interface and creates necessary event groups.

 * the SIM7600/A7670 modem device.
 */
void modem_config(void);

/**
 * @brief Performs mode detection, checks signal quality, 
 * and switches the modem into Data Mode (PPP) to obtain an IP.
 */
void ppp_data_mode_start(void);

/**
 * @brief External handler for IP events. 
 * Note: You must define this in your .c file or main.c 
 * to handle the actual GOT_IP event.
 */


#ifdef __cplusplus
}
#endif

#endif // MY_PPP_H