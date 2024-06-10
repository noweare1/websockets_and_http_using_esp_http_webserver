/*
  WebSocket Echo Server Example (esp-idf examples) but Modified.
  This is part of the IDF examples located in esp_webserver examples

  This program was tested using Postman as a client.

  The goal of this program is to see if  I can use websockets and http protocols using one library.
  This program demonstates the esp_http_server (esp-idf framework) can connect to multiple clients using either ws or http protocols.

  This program demonstrates how the server can broadcast data to all ws clients
  when "Stop frames" or "Start frames" is sent from any connected client
  by using a freetos timer that calls a task peridically. "Stop frames" sent from any client stops the asynchronous sends.

  Any other msg besides "Stop frames" and "Start frames" the server will respond to by appending the client message to the response (like an echo).
  Multiple clients can be connected to the server at the same time either as a websocket client or an http client
  but of course not both at the same time.

  A very simple http response for connected http clients using a GET request is demonstrated.

/* Definitions for error constants. */
/*
#define ESP_OK 0    // < esp_err_t value indicating success (no error)
#define ESP_FAIL -1 // < Generic esp_err_t code indicating failure

#define ESP_ERR_NO_MEM 0x101           // Out of memory
#define ESP_ERR_INVALID_ARG 0x102      // < Invalid argument
#define ESP_ERR_INVALID_STATE 0x103    // < Invalid state
#define ESP_ERR_INVALID_SIZE 0x104     // < Invalid size
#define ESP_ERR_NOT_FOUND 0x105        // < Requested resource not found
#define ESP_ERR_NOT_SUPPORTED 0x106    // < Operation or feature not supported
#define ESP_ERR_TIMEOUT 0x107          // < Operation timed out
#define ESP_ERR_INVALID_RESPONSE 0x108 // < Received response was invalid
#define ESP_ERR_INVALID_CRC 0x109      // < CRC or checksum was invalid
#define ESP_ERR_INVALID_VERSION 0x10A  // < Version was invalid
#define ESP_ERR_INVALID_MAC 0x10B      // < MAC address was invalid
#define ESP_ERR_NOT_FINISHED 0x10C     // < There are items remained to retrieve
*/

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include <WiFi.h>
// #include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
// #include <nvs_flash.h>
#include <sys/param.h>
#include <esp_http_server.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

static httpd_handle_t server = NULL;
static int socketID = 0;
// void ws_asynchronous_send(void);

//************   Global Variables *************//
TaskHandle_t sendMsgTaskHandle;
TimerHandle_t xTimer;
TickType_t timerPeriod = 10000;    // Ticktype_t is uint32_t
TickType_t timerWaitPeriod = 1000; // waits 1 sec to stop the timer if needed
const char *ssid = "boulderhill";
const char *password = "wideflower594";

/*used for get client list below 3 lines are global since they are used in more than one function */
#define MAX_CLIENTS 5
size_t fds = MAX_CLIENTS;
int client_fds[MAX_CLIENTS] = {0};

/*
* Structure holding server handle
* and internal socket fd in order
* to use out of request send

struct async_resp_arg
{
httpd_handle_t hd;
int fd;
};
*/

//-------timer call back for sending async data to client- (freetos software timer callback)
static void prvTimerCallback(TimerHandle_t xTimer) // this is a RTOS timer callback
{
  xTaskNotifyGive(sendMsgTaskHandle);
}

// ask that runs every 10 seconds
static void sendMsgTask(void *param)
{
  for (;;)
  {
    ulTaskNotifyTake(true, portMAX_DELAY);
    ws_asynchronous_send();
    // printOutClients();      //used for troubleshooting
  }
}

// function for sending to client without a request
static void ws_asynchronous_send(void)
{
  esp_err_t ret;
  const char data[50] = "message to all clients";
  httpd_ws_frame_t ws_test_pkt; // we need to define a var of type ws_frame to put our message in
  memset(&ws_test_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_test_pkt.payload = (uint8_t *)data;
  ws_test_pkt.len = strlen(data);
  ws_test_pkt.type = HTTPD_WS_TYPE_TEXT;
  ret = httpd_ws_send_frame_to_all_clients(&ws_test_pkt);
  // httpd_ws_send_frame_async(server, socketID, &ws_test_pkt);    //to get socketId you need the rqst
  if (ret != ESP_OK)
  {
    log_i("ws send frame to all clients", esp_err_to_name(ret));
    return;
  }
  return;
}

static esp_err_t echo_handler(httpd_req_t *req);
// static esp_err_t echo_handler(httpd_req_t *req)
//  HTTP GET Handler, sends index.html to client
static esp_err_t test_get_handler(httpd_req_t *req)
{
  uint8_t res_buf[50] = "Server Response: Hello Client";
  // const uint32_t root_len = root_end - root_start;
  log_i("Serve test handler");

  httpd_resp_set_status(req, "202 OK"); // all this works
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, (char *)res_buf, strlen((char *)res_buf));
  // httpd_resp_sendstr(req, buff;

  /*
  uint32_t heapSize = ESP.getFreeHeap();  //check heap size for memory leak
  log_i("heap size : ", heapSize);
  */
  return ESP_OK;
}

/* USES QUEUE WORK, I HAVE NOT USED THIS FUNCTION YET
trigger_async_send(req->handle, req); PROBLEM WITH THIS IS YOU STILL NEED A REQUEST
static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req) handle and file descriptor
{
  struct async_resp_arg *resp_arg = (async_resp_arg *)malloc(sizeof(struct async_resp_arg));
  resp_arg->hd = req->handle;
  resp_arg->fd = httpd_req_to_sockfd(req);
  esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg); // calls ws_async_send
  if (ret != ESP_OK)
      free(resp_arg);
  return ret;
}
*/

/*  uncomment to test that clients are accounted for. To call perioically use sendMsgTask
static void printOutClients(void)
{
  fds = MAX_CLIENTS; // these two lines needed here since the function overwrites these values
  client_fds[MAX_CLIENTS] = {0};

  // esp_err_t httpd_get_client_list(httpd_handle_t handle, size_t *fds, int *client_fds);
  esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);
  if (ret != ESP_OK)
    log_i("get client list err %s", esp_err_to_name(ret));

  for (int i = 0; i < fds; i++)
    log_i("client %d socket = %d", i, client_fds[i]);
}
*/

// sends a packet to all connected clients, call from
static esp_err_t httpd_ws_send_frame_to_all_clients(httpd_ws_frame_t *ws_pkt)
{
  fds = MAX_CLIENTS; // // these two lines needed here since the function overwrites these values
  client_fds[MAX_CLIENTS] = {0};

  esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);
  if (ret != ESP_OK)
  {
    log_i("get client list err %s", esp_err_to_name(ret));
    return ret;
  }

  for (int i = 0; i < fds; i++)
  {
    int client_info = httpd_ws_get_fd_info(server, client_fds[i]); // gets type of fd
    // log_i("client info =%d", client_info);
    if (client_info == HTTPD_WS_CLIENT_WEBSOCKET)
    {
      esp_err_t ret = httpd_ws_send_frame_async(server, client_fds[i], ws_pkt);
      // delay(10);
      if (ret != ESP_OK)
        log_i("send_frame_async error %s", esp_err_to_name(ret)); // print out actual error code
    }
  }
  // Below can be commented out when all is working
  for (int i = 0; i < fds; i++)
    log_i("client %d socket = %d", i, client_fds[i]);

  return ESP_OK;
}

/*
 * This handler echos back the received websocket data.
 */
static esp_err_t echo_handler(httpd_req_t *req)
{
  uint8_t buf[50] = {0}; // we are skipping dynamic allocation and allocate statically since our data set is small
                         // log_i("buf[49]: %u", buf[49]);

  if (req->method == HTTP_GET)
  {
    log_i("Handshake done, the new connection was opened");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;

  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t)); // init members of ws_pkt to 0
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;             // type will be text

  /* Set max_len = 0 to get the frame len */
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); // setting max_length parameter to 0
  if (ret != ESP_OK)                                    // stores framelength in ws_pkt.len
  {
    log_e("httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }
  log_i("frame len is %d", ws_pkt.len);
  log_i("Packet type: %d", ws_pkt.type);
  ws_pkt.payload = buf;

  if (ws_pkt.len > 0)
  {
    // ws_pkt.payload = buf; // ws_pkt.payload gets assigned same address as buf

    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len); // this call places request data in payload
    if (ret != ESP_OK)
    {
      log_e("httpd_ws_recv_frame failed with %d", ret);
      return ret;
    }
    log_i("Got packet with message: %s", ws_pkt.payload);

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp((char *)ws_pkt.payload, "Stop frames") == 0)
    {
      xTimerStop(xTimer, pdMS_TO_TICKS(timerWaitPeriod));
      log_i("Stopping Asynchronous sends");
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strcmp((char *)ws_pkt.payload, "Start frames") == 0)
    {
      xTimerStart(xTimer, pdMS_TO_TICKS(timerWaitPeriod));
      log_i("Starting Asynchronous sends");
    }
  } // if (ws_pkt.len > 0)
  char temp[50] = "";

  strcpy(temp, (char *)ws_pkt.payload); // cpy payload into temp

  int len = sprintf((char *)buf, "Server rcvd: %s", (char *)temp); // this overwrites start of ws_pkt.payload

  ws_pkt.len = len + 1;
  ret = httpd_ws_send_frame(req, &ws_pkt); // what is the difference between this  and
  if (ret != ESP_OK)
  {
    log_i("httpd_ws_send_frame failed with %d", ret);
  }

  return ret;
}

// stop the running server
static esp_err_t stop_webserver(httpd_handle_t server)
{
  // Stop the httpd server
  return httpd_stop(server);
}

// using event handler to call stop_webserver if we lose wifi connection.
static void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  httpd_handle_t *server = (httpd_handle_t *)arg;
  if (*server)
  {
    log_i("Stopping webserver");
    if (stop_webserver(*server) == ESP_OK)
    {
      *server = NULL;
    }
    else
    {
      log_e("Failed to stop http server");
    }
  }
}

// using event handler to start the webserver if we loose wifi connection. Don't think it is set up correctly because there are
// no retries in code. Should be a case in disconnect_handler to reconnect on an event.
static void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  httpd_handle_t *server = (httpd_handle_t *)arg;
  if (*server == NULL)
  {
    log_i("Starting webserver");
    *server = start_webserver(); // this is where the webserve
  }
}

// uri types
static const httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = echo_handler,
    .user_ctx = NULL,
    .is_websocket = true};

// if this is placed at top of code an error occurs for some reason.
static const httpd_uri_t test = {
    .uri = "/test",
    .method = HTTP_GET,
    .handler = test_get_handler,
    .user_ctx = NULL,
    .is_websocket = false};

// starting webserver, register http and ws end points
static httpd_handle_t start_webserver(void)
{
  server = NULL;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Start the httpd server
  log_i("Starting server on port: '%d'", config.server_port);

  if (httpd_start(&server, &config) == ESP_OK)
  {
    // Registering the ws handler
    log_i("Registering URI handlers");
    httpd_register_uri_handler(server, &ws);
    httpd_register_uri_handler(server, &test);
    return server; // return server handle if all goes well
  }
  log_i("Error starting server!");
  return NULL; // return null if a problem
}

void setup(void)
{
  static httpd_handle_t server = NULL;

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  /* Start the server for the first time */
  server = start_webserver(); // server value should be checked

  xTimer = xTimerCreate("xTimer", pdMS_TO_TICKS(timerPeriod), pdTRUE, (void *)0, prvTimerCallback); // interval timer that call sendMsgTask

  xTaskCreate(&sendMsgTask, "sendMsgTask", 2048, NULL, 3, &sendMsgTaskHandle); // this task broadcasts a message to all connected clients
}

void loop(void)
{
}
