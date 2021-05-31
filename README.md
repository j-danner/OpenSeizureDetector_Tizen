# OpenSeizureDetector-Tizen
Detection of generalized tonic-clonic seizures using a smartwatch using Tizen OS (e.g. Galaxy Watch Active 2) in combination with
the [OpenSeizureDetector App](https://github.com/OpenSeizureDetector/Android_Pebble_SD).


## Implementation Details

The Tizen-app is split into two parts. The first is a native service app (written in C) that runs _reliable_ in the background and takes care of starting the BLE GATT server its
services and sharing movemenent data over it. The other is a web app - the UI - and is responsible for a handful of settings, and - most importantly - starting and stopping of
the service app.

Below you can find a task-list which indicates the current state of the project and the goals for the near future:

- [Tizen Native Service Application](https://docs.tizen.org/application/native/guides/applications/service-app/):
  - [x] send notification on shutdown (if not triggered by UI) to detect when it _crashes_
  - [x] initialize GATT server and register services for 
    - [x] battery level
    - [x] accelerometer data
    - [ ] HRM
  - [x] record acc data with 25 Hz and send it via GATT service to phone every second
  - [ ] record HRM data and send it via GATT service to phone every second (optional, might affect battery significantly)
  - [ ] find good sample frequency (50Hz?) and ensure OSD-App supports it

- [Tizen Web Application](https://docs.tizen.org/application/web/index) (javascript):
  - [x] UI that allows starting and stopping of seizure detection of native service app
  - [x] check on start of UI if service app is running (and initialize button correctly!)
  - [x] if something goes wrong in the service app (e.g. it crashes), give appropriate popup warnings in UI
  - [ ] popup if phone is disconnected (Bluetooth) for too long (e.g. 1 minute)











