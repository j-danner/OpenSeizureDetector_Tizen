#include <tizen.h>
#include <service_app.h>
#include "openseizuredetector_tizen.h"

// headers that will be needed for our service:
//stdlib...
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>
#include <string.h>
//tizen...
#include <sensor.h>
#include <device/power.h>
#include <device/battery.h>
#include <device/haptic.h>
#include <notification.h>

//RingBuffer
#include <rb.h>

//debugging...
#include <assert.h>

//BLE
#include <bluetooth.h>



// some constant values used in the app
#define MYSERVICELAUNCHER_APP_ID "iPXnvs5fcO.OpenSeizureDetector" // an ID of the UI application of our package
#define STRNCMP_LIMIT 256 // the limit of characters to be compared using strncmp function

#define sampleRate 25 //in Hz (no samples per sec), this leads an interval of 1/sampleRate secs between measurements
#define dataQueryInterval 40 //time in ms between measurements //only possible to do with 20, 40, 60, 80, 100, 200, 300 (results from experiments)

// BLE GATT UUID constants
//#define UUID_START    "a19585e9"
#define UUID_SERVICE  "a19585e9-0001-39d0-015f-b3e2b9a0c854"
#define UUID_CHAR_ACC "a19585e9-0002-39d0-015f-b3e2b9a0c854"
#define UUID_CHAR_HRM "a19585e9-0003-39d0-015f-b3e2b9a0c854"
#define UUID_CHAR_BAT "a19585e9-0004-39d0-015f-b3e2b9a0c854"


// application data (context) that will be passed to functions when needed
typedef struct appdata
{
  sensor_h sensor; // sensor handle
  sensor_listener_h listener; // sensor listener handle, must always be NULL if listener is not running!

  //circular buffers for x, y, and z linear acc data
  ringbuf_t rb_x;
  ringbuf_t rb_y;
  ringbuf_t rb_z;

  //buffers for data
  double* acc_buff;
  double* HRM_buff;

  //notification handles
  notification_h shutdown_notification;

  //BLE handles
  bt_gatt_server_h bt_server;
  bt_gatt_h bt_service;
  bt_gatt_h bt_char_acc;
  bt_gatt_h bt_char_hrm;
  bt_gatt_h bt_char_bat;

  bt_gatt_h bt_desc_cccd_acc;
  bt_gatt_h bt_desc_cccd_bat;
  bt_gatt_h bt_desc_cccd_hrm;

  bt_advertiser_h bt_advertiser;

} appdata_s;


//cb funcs
void __bt_gatt_server_read_value_requested_cb (const char *remote_address,
                int request_id, bt_gatt_server_h server, bt_gatt_h gatt_handle,
                int offset, void *user_data)
{
    dlog_print(DLOG_ERROR, LOG_TAG,"__bt_gatt_server_read_value_requested_cb");
    dlog_print(DLOG_ERROR, LOG_TAG,"remote_address %s", remote_address);
    dlog_print(DLOG_ERROR, LOG_TAG,"req_id %d", request_id);
    dlog_print(DLOG_ERROR, LOG_TAG,"server %s", (char *)server);
    dlog_print(DLOG_ERROR, LOG_TAG,"gatt_handle %s", (char *)gatt_handle);
    dlog_print(DLOG_ERROR, LOG_TAG,"Offset %d", offset);

    //local buffer for current value
    char* value = " ";
    int len = 1;

    int ret = bt_gatt_get_value(gatt_handle, &value, &len);
    dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_get_value: %s \n", get_error_message(ret));

    /* Get the attribute new values here */
    ret = bt_gatt_server_send_response(request_id, BT_GATT_REQUEST_TYPE_READ, offset, BT_ERROR_NONE, value, len - offset);
    dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_send_response : %s \n", get_error_message(ret));

    //free value
    free(value);
}

void __bt_gatt_server_write_value_requested_cb (const char *remote_address,
                    int request_id, bt_gatt_server_h server, bt_gatt_h gatt_handle,
                    bool response_needed, int offset, const char *value, int len, void *user_data)
{
    dlog_print(DLOG_ERROR, LOG_TAG,"__bt_gatt_server_write_value_requested_cb");
    dlog_print(DLOG_ERROR, LOG_TAG,"remote_address %s", remote_address);
    dlog_print(DLOG_ERROR, LOG_TAG,"req_id %d", request_id);
    dlog_print(DLOG_ERROR, LOG_TAG,"server %s", (char *)server);
    dlog_print(DLOG_ERROR, LOG_TAG,"gatt_handle %s", (char *)gatt_handle);
    dlog_print(DLOG_ERROR, LOG_TAG,"Offset %d", offset);

    // Set the new attribute values -- only replaces the full value, i.e., offsets!=0 not supported as of now.
    if(offset!=0) {
        dlog_print(DLOG_ERROR, LOG_TAG, "offset is non-zero! UNSUPPORTED ACTION -> send failure indication");
        //send failure response
        int ret = bt_gatt_server_send_response(request_id, BT_GATT_REQUEST_TYPE_WRITE, 0, BT_ERROR_NONE, "1", 1);
        dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_send_response : %s \n", get_error_message(ret));
    }

    int ret = bt_gatt_set_value(gatt_handle, value, len);
    dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_set_characteristic_value : %s \n", get_error_message(ret));

    if (ret != BT_ERROR_NONE) {
        //send success response
        ret = bt_gatt_server_send_response(request_id, BT_GATT_REQUEST_TYPE_WRITE, 0, BT_ERROR_NONE, "0", 1);
        dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_send_response : %s \n", get_error_message(ret));
    } else {
        //send failure response
        ret = bt_gatt_server_send_response(request_id, BT_GATT_REQUEST_TYPE_WRITE, 0, BT_ERROR_NONE, "1", 1);
        dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_send_response : %s \n", get_error_message(ret));
    }

}

static void __bt_adapter_le_advertising_state_changed_cb(int result,
        bt_advertiser_h advertiser, bt_adapter_le_advertising_state_e adv_state, void *user_data)
{
    dlog_print(DLOG_ERROR, LOG_TAG, "Result : %d", result);
    dlog_print(DLOG_ERROR, LOG_TAG, "Advertiser : %p", advertiser);
    dlog_print(DLOG_ERROR, LOG_TAG, "Advertising %s [%d]", adv_state == BT_ADAPTER_LE_ADVERTISING_STARTED ?
                "started" : "stopped", adv_state);
}

void __bt_gatt_server_notification_state_change_cb(bool notify,
            bt_gatt_server_h server, bt_gatt_h gatt_handle, void *user_data)
{
    // Extracting application data
    appdata_s* ad = (appdata_s*)user_data;

    dlog_print(DLOG_ERROR, LOG_TAG, "__bt_gatt_server_notification_state_change_cb");
    dlog_print(DLOG_ERROR, LOG_TAG, "notify %d", notify);
    dlog_print(DLOG_ERROR, LOG_TAG, "server %s", (char *)server);
    dlog_print(DLOG_ERROR, LOG_TAG, "gatt_handle %s", (char *)gatt_handle);

    if (notify) {
        //notification enabled -> stop advertising
        int err = bt_adapter_le_stop_advertising(ad->bt_advertiser);
        if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not stop ble advertiser!");
    } else {
        //notification disabled -> start advertising
        int err = bt_adapter_le_start_advertising_new(ad->bt_advertiser, __bt_adapter_le_advertising_state_changed_cb, NULL);
        if (err != BT_ERROR_NONE) {
            dlog_print(DLOG_WARN, LOG_TAG, "could not start ble advertisement!");
        } else {
            dlog_print(DLOG_INFO, LOG_TAG, "started ble advertisement!");
        }
    }
}


void __bt_gatt_server_notification_sent_cb(int result, const char *remote_address, bt_gatt_server_h server,
          bt_gatt_h characteristic, bool completed, void *user_data)
{
    dlog_print(DLOG_ERROR, LOG_TAG, "__bt_gatt_server_notification_sent_cb");
    dlog_print(DLOG_ERROR, LOG_TAG, "result %d", result);
    dlog_print(DLOG_ERROR, LOG_TAG, "remote_address %s", remote_address);
    dlog_print(DLOG_ERROR, LOG_TAG, "server %s", (char *)server);
    dlog_print(DLOG_ERROR, LOG_TAG, "characteristic %s", (char *)characteristic);
    dlog_print(DLOG_ERROR, LOG_TAG, "completed %i", completed);

}


//publish notification
void publish_notification(notification_h noti) {
  int noti_err = notification_post(noti);
  if (noti_err != NOTIFICATION_ERROR_NONE) { dlog_print(DLOG_ERROR, LOG_TAG, "could not post shutdown notification!"); return; }
}


void start_UI()
{
  app_control_h app_control = NULL;
  if (app_control_create(&app_control) == APP_CONTROL_ERROR_NONE)
  {
    //Setting an app ID.
    if (app_control_set_app_id(app_control, MYSERVICELAUNCHER_APP_ID) == APP_CONTROL_ERROR_NONE)
    {
      if(app_control_send_launch_request(app_control, NULL, NULL) == APP_CONTROL_ERROR_NONE)
      {
        dlog_print(DLOG_INFO, LOG_TAG, "App launch request sent!");
      }
    }
    if (app_control_destroy(app_control) == APP_CONTROL_ERROR_NONE)
    {
      dlog_print(DLOG_INFO, LOG_TAG, "App control destroyed.");
    }
  }
}



//updates data of BLE GATT service -- also updates battery level
void share_data(void *data)
{
    // Extracting application data
    appdata_s* ad = (appdata_s*)data;

    //update data of BLE service
    dlog_print(DLOG_INFO, LOG_TAG, "share_data: start!");

    //combine values and round to ints

    //get correctly rotated buffers
    double buff_x[sampleRate];
    double buff_y[sampleRate];
    double buff_z[sampleRate];

    ringbuf_get_buf(ad->rb_x, buff_x);
    ringbuf_get_buf(ad->rb_y, buff_y);
    ringbuf_get_buf(ad->rb_z, buff_z);

    double comb_val[sampleRate];
    char comb_val_char[sampleRate];
    //combine values (compute magnitude)
    for(int i=0; i<sampleRate; i++) {
        comb_val[i] = sqrt( pow(buff_x[i],2) + pow(buff_y[i],2) + pow(buff_z[i],2) );
        comb_val_char[i] = round(comb_val[i]);

        dlog_print(DLOG_INFO, LOG_TAG, "comb_val[%i] = %f, comb_val_char[%i]=%i", i, comb_val[i], i, comb_val_char[i]);
    }

    //read battery status
    char battery_status[1] = "0";
    device_battery_get_percent((int*)battery_status);

    //share data via BLE
    //update value of bt_char_acc
    int ret = bt_gatt_set_value(ad->bt_char_acc, comb_val_char, sampleRate);
    dlog_print(DLOG_INFO, LOG_TAG, "bt_gatt_set_value acc : %s\n", get_error_message(ret));

    //notify all devices with activated indication/notification -- TODO: do we need to send indication of this? (OSD only requests read of value every minute.)
    ret = bt_gatt_server_notify_characteristic_changed_value(ad->bt_char_acc, __bt_gatt_server_notification_sent_cb, NULL, NULL);
    dlog_print(DLOG_INFO, LOG_TAG, "bt_gatt_server_notify_characteristic_changed_value acc : %s\n", get_error_message(ret));

    //update value of bt_char_bat
    ret = bt_gatt_set_value(ad->bt_char_bat, (char*) battery_status, 2);
    dlog_print(DLOG_INFO, LOG_TAG, "bt_gatt_set_value bat : %s\n", get_error_message(ret));

    //notify all devices with activated indication/notification
    ret = bt_gatt_server_notify_characteristic_changed_value(ad->bt_char_bat, __bt_gatt_server_notification_sent_cb, NULL, NULL);
    dlog_print(DLOG_INFO, LOG_TAG, "bt_gatt_server_notify_characteristic_changed_value bat : %s\n", get_error_message(ret));


    dlog_print(DLOG_INFO, LOG_TAG, "share_data: end.");
}

//sensor event callback implementation, to be called every time sensor data is received!
void sensor_event_callback(sensor_h sensor, sensor_event_s *event, void *data)
{
  // Extracting application data
  appdata_s* ad = (appdata_s*)data;

  sensor_type_e type = SENSOR_ALL;

  if((sensor_get_type(sensor, &type) == SENSOR_ERROR_NONE) && type == SENSOR_ACCELEROMETER)
  {
    ringbuf_push(ad->rb_x, event->values[0]);
    ringbuf_push(ad->rb_y, event->values[1]);
    ringbuf_push(ad->rb_z, event->values[2]);

    //each second perform share data with android phone via BLE
    if((ad->rb_x)->idx % sampleRate == 0)
    {
        share_data(ad);
    }
  }
}


//called on service app creation (initialize rbs and BLE server/chars/descrs/adv etc)
bool service_app_create(void *data)
{
  // Extracting application data
  appdata_s* ad = (appdata_s*)data;

  bool sensor_supported = false;
  if (sensor_is_supported(SENSOR_ACCELEROMETER, &sensor_supported) != SENSOR_ERROR_NONE || sensor_supported == false)
  {
    dlog_print(DLOG_ERROR, LOG_TAG, "Accelerometer not supported! Service is useless, exiting...");
    //TODO notify user over UI?!
    service_app_exit();
    return false;
  }

  ad->listener = NULL; //make sure that at startup listener is NULL, i.e., isRunning indicates that listener is not registered!

  //create ringbuffs
  ad->rb_x = ringbuf_new(sampleRate);
  ad->rb_y = ringbuf_new(sampleRate);
  ad->rb_z = ringbuf_new(sampleRate);
  dlog_print(DLOG_INFO, LOG_TAG, "ringbufs created.");

  ad->acc_buff = malloc(sizeof(double)*sampleRate);
  ad->HRM_buff = malloc(sizeof(double)*sampleRate);

  //TODO initialize BLE GATT server

  //make sure bluetooth is running
  int ret = bt_initialize();
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_initialize : %s \n", get_error_message(ret));
  bt_adapter_state_e state = BT_ADAPTER_DISABLED;
  ret = bt_adapter_get_state(&state);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_adapter_get_state : %s \n", get_error_message(ret));
  if (state != BT_ADAPTER_ENABLED) {
      dlog_print(DLOG_ERROR, LOG_TAG, "could not enable bt adapter!");
      //TODO INFORM UI THAT BT IS NOT ENABLED!
  }


  ret = bt_gatt_server_initialize();
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_initialize : %s \n", get_error_message(ret));

  ret = bt_gatt_server_create(&ad->bt_server);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_create : %s \n", get_error_message(ret));


  ret = bt_gatt_service_create(UUID_SERVICE, BT_GATT_SERVICE_TYPE_PRIMARY, &ad->bt_service);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_service_create : %s \n", get_error_message(ret));


  //add chars to service
  //(1) char_acc
  ret = bt_gatt_characteristic_create(UUID_CHAR_ACC, BT_GATT_PERMISSION_READ,
          BT_GATT_PROPERTY_WRITE | BT_GATT_PROPERTY_READ | BT_GATT_PROPERTY_NOTIFY, "___EMPTY__INITIAL__DATA___", sampleRate*sizeof(char), &ad->bt_char_acc);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_characteristic_create : %s\n", get_error_message(ret));

  //register cbs of characteristic!
  ret = bt_gatt_server_set_read_value_requested_cb(ad->bt_char_acc, __bt_gatt_server_read_value_requested_cb, NULL);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_set_read_value_requested_cb : %s\n", get_error_message(ret));
  //TODO as writing is not allowed, we might remove this cb ?!
  ret = bt_gatt_server_set_write_value_requested_cb(ad->bt_char_acc, __bt_gatt_server_write_value_requested_cb, NULL);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_set_write_value_requested_cb : %s\n", get_error_message(ret));

  ret = bt_gatt_server_set_characteristic_notification_state_change_cb(ad->bt_char_acc, __bt_gatt_server_notification_state_change_cb, ad);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_set_characteristic_notification_state_change_cb : %s\n", get_error_message(ret));

  char CCCD_value[2] = {0, 0}; // Notification & Indication disabled
  ret = bt_gatt_descriptor_create("2902", BT_GATT_PERMISSION_READ | BT_GATT_PERMISSION_WRITE, CCCD_value, sizeof(CCCD_value), &ad->bt_desc_cccd_acc);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_descriptor_create : %s\n", get_error_message(ret));

  ret = bt_gatt_characteristic_add_descriptor(ad->bt_char_acc, ad->bt_desc_cccd_acc);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_characteristic_add_descriptor : %s\n", get_error_message(ret));

  //register char in service
  ret = bt_gatt_service_add_characteristic(ad->bt_service, ad->bt_char_acc);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_service_add_characteristic : %s\n", get_error_message(ret));

  //(2) char_bat
  ret = bt_gatt_characteristic_create(UUID_CHAR_BAT, BT_GATT_PERMISSION_READ,
          BT_GATT_PROPERTY_WRITE | BT_GATT_PROPERTY_READ | BT_GATT_PROPERTY_NOTIFY, "0", sizeof(char), &ad->bt_char_bat);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_characteristic_create : %s\n", get_error_message(ret));

  //register cbs of characteristic!
  ret = bt_gatt_server_set_read_value_requested_cb(ad->bt_char_bat, __bt_gatt_server_read_value_requested_cb, NULL);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_set_read_value_requested_cb : %s\n", get_error_message(ret));
  //TODO as writing is not allowed, we might remove this cb ?!
  ret = bt_gatt_server_set_write_value_requested_cb(ad->bt_char_bat, __bt_gatt_server_write_value_requested_cb, NULL);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_set_write_value_requested_cb : %s\n", get_error_message(ret));

  ret = bt_gatt_server_set_characteristic_notification_state_change_cb(ad->bt_char_bat, __bt_gatt_server_notification_state_change_cb, ad);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_set_characteristic_notification_state_change_cb : %s\n", get_error_message(ret));

  ret = bt_gatt_descriptor_create("2902", BT_GATT_PERMISSION_READ | BT_GATT_PERMISSION_WRITE, CCCD_value, sizeof(CCCD_value), &ad->bt_desc_cccd_bat);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_descriptor_create : %s\n", get_error_message(ret));

  ret = bt_gatt_characteristic_add_descriptor(ad->bt_char_bat, ad->bt_desc_cccd_bat);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_characteristic_add_descriptor : %s\n", get_error_message(ret));

  //register char in service
  ret = bt_gatt_service_add_characteristic(ad->bt_service, ad->bt_char_bat);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_service_add_characteristic : %s\n", get_error_message(ret));

  //(3) char_HRM
  //TODO


  //register service in server!
  ret = bt_gatt_server_register_service(ad->bt_server, ad->bt_service);
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_register_service : %s\n", get_error_message(ret));

  ret = bt_gatt_server_start();
  dlog_print(DLOG_ERROR, LOG_TAG, "bt_gatt_server_start : %s\n", get_error_message(ret));


  //create advertiser, it is started in sensor_start, and stopped at the latest in sensor_stop
  ret = bt_adapter_le_create_advertiser(&ad->bt_advertiser);
  dlog_print(DLOG_ERROR, LOG_TAG, "create advertiser: %s \n", get_error_message(ret));

  bt_adapter_le_add_advertising_service_uuid(ad->bt_advertiser, BT_ADAPTER_LE_PACKET_ADVERTISING, UUID_SERVICE);
  dlog_print(DLOG_ERROR, LOG_TAG, "added service uuid to advertiser: %s \n", get_error_message(ret));

  ret = bt_adapter_le_set_advertising_device_name(ad->bt_advertiser, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, true);
  dlog_print(DLOG_ERROR, LOG_TAG, "enabling advertising device name: %s\n", get_error_message(ret));

  ret = bt_adapter_le_set_advertising_mode(ad->bt_advertiser, BT_ADAPTER_LE_ADVERTISING_MODE_LOW_ENERGY);
  dlog_print(DLOG_ERROR, LOG_TAG, "set advertising mode to 'low-energy': %s\n", get_error_message(ret));


  //create notification handles for unexpected shutdown notification!
  ad->shutdown_notification = notification_create(NOTIFICATION_TYPE_NOTI);
  if (ad->shutdown_notification == NULL) dlog_print(DLOG_ERROR, LOG_TAG, "could not create shutdown notification!");
  int noti_err = NOTIFICATION_ERROR_NONE;
  noti_err = notification_set_text(ad->shutdown_notification, NOTIFICATION_TEXT_TYPE_TITLE, "Warning!", "EPILARM_NOTIFICATION_TITLE", NOTIFICATION_VARIABLE_TYPE_NONE);
  if (noti_err != NOTIFICATION_ERROR_NONE) dlog_print(DLOG_ERROR, LOG_TAG, "could not create title of shutdown notification!");
  noti_err = notification_set_text(ad->shutdown_notification, NOTIFICATION_TEXT_TYPE_CONTENT, "Unscheduled shutdown of Epilarm!\n (Restart via Epilarm app!)",
      "EPILARM_NOTIFICATION_CONTENT", NOTIFICATION_VARIABLE_TYPE_NONE);
  if (noti_err != NOTIFICATION_ERROR_NONE) dlog_print(DLOG_ERROR, LOG_TAG, "could not create content of shutdown notification!");
  noti_err = notification_set_vibration(ad->shutdown_notification, NOTIFICATION_VIBRATION_TYPE_DEFAULT, NULL);
  if (noti_err != NOTIFICATION_ERROR_NONE) dlog_print(DLOG_ERROR, LOG_TAG, "could not set vibration of shutdown notification!");

  return true;
}

//checks whether analysis is running, by checking if sensor listener is running
int is_running(void *data)
{
  // Extracting application data
  appdata_s* ad = (appdata_s*)data;

  return ad->listener != NULL;
}


//starts the sensor with the seizure detection algorithm, and starts BLE advertisement
void sensor_start(void *data)
{
  // Extracting application data
  appdata_s* ad = (appdata_s*)data;

  // Preparing and starting the sensor listener for the accelerometer.
  if (sensor_get_default_sensor(SENSOR_ACCELEROMETER, &(ad->sensor)) == SENSOR_ERROR_NONE)
  {
    if (sensor_create_listener(ad->sensor, &(ad->listener)) == SENSOR_ERROR_NONE
      && sensor_listener_set_event_cb(ad->listener, dataQueryInterval, sensor_event_callback, ad) == SENSOR_ERROR_NONE
      && sensor_listener_set_option(ad->listener, SENSOR_OPTION_ALWAYS_ON) == SENSOR_ERROR_NONE
      && sensor_listener_set_attribute_int(ad->listener, SENSOR_ATTRIBUTE_PAUSE_POLICY, SENSOR_PAUSE_NONE) == SENSOR_ERROR_NONE
      && device_power_request_lock(POWER_LOCK_CPU, 0) == DEVICE_ERROR_NONE)
    {
      if (sensor_listener_start(ad->listener) == SENSOR_ERROR_NONE)
      {
        dlog_print(DLOG_INFO, LOG_TAG, "Sensor listener started.");

        int err = bt_adapter_le_start_advertising_new(ad->bt_advertiser, __bt_adapter_le_advertising_state_changed_cb, NULL);
        if (err != BT_ERROR_NONE) {
            dlog_print(DLOG_WARN, LOG_TAG, "could not start ble advertisement!");
        } else {
            dlog_print(DLOG_INFO, LOG_TAG, "started ble advertisement!");
        }

      } else {
          //TODO inform user that sensor listener could not be started?!
      }
    }
  }
}


//stops the sensor, ble advertiser, and stops sharing of data
//input: appdata, bool that tells if a warning notification should be sent (e.g. if it the sensor is not stopped by UI)
void sensor_stop(void *data, bool warn)
{
  // Extracting application data
  appdata_s* ad = (appdata_s*)data;

  if(!is_running(ad)) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Sensor listener already destroyed.");
    return;
  }

  //stop ble advertiser
  int err = bt_adapter_le_stop_advertising(ad->bt_advertiser);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not stop ble advertisement!");


  //Stopping & destroying sensor listener
  if ((sensor_listener_stop(ad->listener) == SENSOR_ERROR_NONE)
    && (sensor_destroy_listener(ad->listener) == SENSOR_ERROR_NONE))
  {
    ad->listener = NULL;
    dlog_print(DLOG_INFO, LOG_TAG, "Sensor listener destroyed.");
    if (warn) { //send notification that sensorlistener is destroyed
      dlog_print(DLOG_INFO, LOG_TAG, "Unscheduled shutdown of Epilarm-service! (Restart via UI!)");
      publish_notification(ad->shutdown_notification);
      //TODO start UI and tell it that service app crashed!
      start_UI();
    }
  }
  else
  {
    dlog_print(DLOG_INFO, LOG_TAG, "Error occurred when destroying sensor listener: listener was never created!");
  }

  if(device_power_release_lock(POWER_LOCK_CPU) != DEVICE_ERROR_NONE) {
    dlog_print(DLOG_INFO, LOG_TAG, "could not release cpu lock!");
  }

}

//called when service app is terminated (this is NOT called when sensor listener is destroyed and BHE data sharing is stopped!)
void service_app_terminate(void *data)
{
  // Extracting application data
  appdata_s* ad = (appdata_s*)data;

  sensor_stop(data, true); //if sensor is stopped - with a warning; if already stopped before, no warning

  //destroy ringbufs
  ringbuf_free(&ad->rb_x);
  ringbuf_free(&ad->rb_y);
  ringbuf_free(&ad->rb_z);

  //destroy buffers for data (movement, HRM)
  free(ad->acc_buff);
  free(ad->HRM_buff);

  //destroy BLE GATT server handles and advertiser
  int err = bt_adapter_le_clear_advertising_data(ad->bt_advertiser, BT_ADAPTER_LE_PACKET_ADVERTISING);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not clear ble advertisement data!");

  err = bt_adapter_le_destroy_advertiser(ad->bt_advertiser);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy ble advertisement!");

  err = bt_gatt_server_destroy(&ad->bt_server); //also unregisters services!
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy Bt gatt server!");

  err = bt_gatt_service_destroy(ad->bt_service);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy Bt gatt service!");

  err = bt_gatt_characteristic_destroy(ad->bt_char_acc);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy bt_char_acc!");

  err = bt_gatt_characteristic_destroy(ad->bt_char_hrm);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy bt_char_hrm!");

  err = bt_gatt_characteristic_destroy(ad->bt_char_bat);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy bt_char_bat!");

  err = bt_gatt_descriptor_destroy(ad->bt_desc_cccd_acc);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy bt_desc_cccd_acc!");

  err = bt_gatt_descriptor_destroy(ad->bt_desc_cccd_bat);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy bt_desc_cccd_bat!");

  err = bt_gatt_descriptor_destroy(ad->bt_desc_cccd_hrm);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy bt_desc_cccd_hrm!");

  err = bt_gatt_server_deinitialize();
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not deinit Bt gatt server!");

  err = bt_adapter_le_stop_advertising(ad->bt_advertiser);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not stop ble advertiser!");

  err = bt_adapter_le_destroy_advertiser(ad->bt_advertiser);
  if (err != BT_ERROR_NONE) dlog_print(DLOG_WARN, LOG_TAG, "could not destroy ble advertiser!");



  //free notification handle
  int noti_err = notification_free(ad->shutdown_notification);
  if (noti_err != NOTIFICATION_ERROR_NONE) { dlog_print(DLOG_WARN, LOG_TAG, "could not free shutdown notification!"); return; }
}

//handles incoming appcontrols from UI (e.g. starting/stopping analysis, ftp upload, compression, etc..)
void service_app_control(app_control_h app_control, void *data)
{
  // Extracting application data
  appdata_s* ad = (appdata_s*)data;

  char *caller_id = NULL, *action_value = NULL;
  if ((app_control_get_caller(app_control, &caller_id) == APP_CONTROL_ERROR_NONE)
        && (app_control_get_extra_data(app_control, "service_action", &action_value) == APP_CONTROL_ERROR_NONE))
  {
    dlog_print(DLOG_INFO, LOG_TAG, "caller_id = %s", caller_id);
    dlog_print(DLOG_INFO, LOG_TAG, "action_value = %s", action_value);

    //perform adequate actions
    if((caller_id != NULL) && (action_value != NULL)
             && (!strncmp(caller_id, MYSERVICELAUNCHER_APP_ID, STRNCMP_LIMIT))
             && (!strncmp(action_value, "start", STRNCMP_LIMIT)))
    {
      //// >>>> START SERVICE APP <<<< ////
      dlog_print(DLOG_INFO, LOG_TAG, "Epilarm start! reading params...");
      if(!is_running(ad)) {
        dlog_print(DLOG_INFO, LOG_TAG, "Starting epilarm sensor service!");
        sensor_start(ad); //TODO add return value in case starting of sensor listener fails!
      } else {
        dlog_print(DLOG_INFO, LOG_TAG, "Service already running! Not started again!");
      }

    } else if((caller_id != NULL) && (action_value != NULL)
             && (!strncmp(caller_id, MYSERVICELAUNCHER_APP_ID, STRNCMP_LIMIT))
             && (!strncmp(action_value, "stop", STRNCMP_LIMIT)))
    {
      //// >>>> STOP SERVICE APP <<<< ////
      dlog_print(DLOG_INFO, LOG_TAG, "Stopping epilarm sensor service!");
      sensor_stop(data, false); //stop sensor listener without notification (as it was shut down on purpose)

    } else if((caller_id != NULL) && (action_value != NULL)
                && (!strncmp(caller_id, MYSERVICELAUNCHER_APP_ID, STRNCMP_LIMIT))
                && (!strncmp(action_value, "running?", STRNCMP_LIMIT)))
    {
      //// >>>> CHECK IF ANALYSIS IS RUNNING <<<< ////
      dlog_print(DLOG_INFO, LOG_TAG, "are we running? (asked by UI)!");

      char *app_id;
      app_control_h reply;
      app_control_create(&reply);
      app_control_get_app_id(app_control, &app_id);
      app_control_add_extra_data(reply, APP_CONTROL_DATA_SELECTED, is_running(ad) ? "1" : "0");
      app_control_reply_to_launch_request(reply, app_control, APP_CONTROL_RESULT_SUCCEEDED);
      dlog_print(DLOG_INFO, LOG_TAG, "reply sent (%d)", is_running(ad));
      app_control_destroy(reply);

      free(app_id);

    } else {
      dlog_print(DLOG_INFO, LOG_TAG, "Unsupported action! Doing nothing...");
    }

    free(caller_id);
    free(action_value);
    return;
  }
}

//called when language is changed (??)
static void service_app_lang_changed(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LANGUAGE_CHANGED*/
  return;
}

//called when ???
static void service_app_region_changed(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_REGION_FORMAT_CHANGED*/
  return;
}

//called when battery is low => do nothing TODO should we do sth?!
static void service_app_low_battery(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LOW_BATTERY*/
  return;
}

//called when memory is low => stop logging if it was activated!
static void service_app_low_memory(app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LOW_MEMORY*/
  return;
}


int main(int argc, char* argv[])
{
  // we declare application data as a structure defined earlier
  appdata_s ad = {0,};

  service_app_lifecycle_callback_s event_callback = {0,};
  app_event_handler_h handlers[5] = {NULL, };

  event_callback.create = service_app_create;
  event_callback.terminate = service_app_terminate;
  event_callback.app_control = service_app_control;

  service_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, service_app_low_battery, &ad);
  service_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, service_app_low_memory, &ad);
  service_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, service_app_lang_changed, &ad);
  service_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, service_app_region_changed, &ad);

  // we keep a template code above and then modify the line below
  return service_app_main(argc, argv, &event_callback, &ad);
}
