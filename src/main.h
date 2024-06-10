
#include <esp_http_server.h>
#include <esp_event_base.h>
/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg
{
  httpd_handle_t hd;
  int fd;
};
// declaration in .h file:
#ifdef __cplusplus
extern "C"
{
#endif
  static void ws_asynchronous_send(void);
  // static void ws_async_send(void *);

  static esp_err_t trigger_async_send(httpd_handle_t, httpd_req_t *);

  static esp_err_t echo_handler(httpd_req_t *);

  static httpd_handle_t start_webserver(void);

  static esp_err_t stop_webserver(httpd_handle_t);

  static void disconnect_handler(void *, esp_event_base_t, int32_t, void *);

  static void connect_handler(void *, esp_event_base_t, int32_t, void *);

  static void printOutClients(void);

  static esp_err_t test_get_handler(httpd_req_t);

  static esp_err_t httpd_ws_send_frame_to_all_clients(httpd_ws_frame_t *);

#ifdef __cplusplus
}
#endif