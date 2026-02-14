#include <string>
#ifndef CONFIG_SERVER_H
#define CONFIG_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

void run_config_server();
bool is_config_file_present();
std::string load_file_to_string();

#ifdef __cplusplus
}
#endif

#endif // CONFIG_SERVER_H