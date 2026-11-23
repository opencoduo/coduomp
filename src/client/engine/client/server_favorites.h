#ifndef CODUOMP_SERVER_FAVORITES_H
#define CODUOMP_SERVER_FAVORITES_H

#include "server_browser.h"

/* NOT_FROM_ORIGINAL_SOURCE: persistent user favorites are independent of the
 * disposable browser cache. Loading establishes whether saving is safe. */
void coduomp_load_favorites(lan_server_info_t *servers, int32_t *count);
void coduomp_save_favorites(const lan_server_info_t *servers, int32_t count);

#endif
