//parameters, must be initialized by reset_params() or loaded from localstorage. Simply use load_params() once!
var params; //currently only stores info on automatic start and stop of movement data collection

//list for UI elements
var list;

//store references to html objects of items
var start_stop;
var start_stop_checkbox;

var automatic_analysis;
var automatic_analysis_checkbox;

//settings
var settings_alarm_start;
var settings_alarm_stop;


var SERVICE_APP_ID = tizen.application.getCurrentApplication().appInfo.packageId + '.openseizuredetector_tizen'; //'QOeM6aBGp0.epilarm_sensor_service';
var APP_ID = tizen.application.getCurrentApplication().appInfo.id;


function reset_params() {
	//default values
    params = {
        //alarm info
        alarm: false,
        alarm_start_date: new Date(Date.parse("2020-11-23T06:00")),
        alarm_stop_date: new Date(Date.parse("2020-11-23T18:00")),
        alarm_days: ["MO", "TU", "WE", "TH", "FR", "SA", "SU"],
    };
}

//must be called on startup of UI
function load_params() {
  if ('localStorage' in window) {
      // params that can be changed
      if (localStorage.getItem('params') === null) {
          console.log('load_params: no saved params found, using default values!');
          //use default values!
          reset_params();
      } else {
          params = JSON.parse(localStorage.getItem('params'));
          console.log('params loaded!');
      }
  } else {
      console.log('load_params: no localStorage in window found, using default values!');
      reset_params();
  }
}


//start service app and seizure detection (also makes sure that the corr checkbox is in the correct position!)
function start_service_app() {	
    console.log('starting service app...');
    var obj = new tizen.ApplicationControlData('service_action', ['start']);
    var obj1 = new tizen.ApplicationControl('http://tizen.org/appcontrol/operation/service',
        null,
        null,
        null, [obj], 'SINGLE'
    );
    try {
        tizen.application.launchAppControl(obj1,
            SERVICE_APP_ID,
            function() {
                console.log('Launch Service succeeded');
                
                update_seizure_detection_checkbox();
            },
            function(e) {
                console.log('Launch Service failed : ' + e.message);
                tau.openPopup('#StartFailedPopup');
                
                update_seizure_detection_checkbox();
            }, null);
    } catch (e) {
        window.alert('Error when starting appcontrol for starting seizure detection! error msg:' + e.toString());
        
        update_seizure_detection_checkbox();
    }
}

//stop service app and seizure detection (also makes sure that the corr checkbox is in the correct position!)
function stop_service_app() {
    console.log('stopping service app...');
    var obj = new tizen.ApplicationControlData('service_action', ['stop']); //you'll find the app id in config.xml file.
    var obj1 = new tizen.ApplicationControl('http://tizen.org/appcontrol/operation/service',
        null,
        null,
        null, [obj], 'SINGLE'
    );
    try {
        tizen.application.launchAppControl(obj1,
            SERVICE_APP_ID,
            function() {
                console.log('Stopping Service Request succeeded');
                
                update_seizure_detection_checkbox();
            },
            function(e) {
                console.log('Stopping Service Request failed : ' + e.message);
                
                update_seizure_detection_checkbox();
            }, null);
    } catch (e) {
        window.alert('Error when starting appcontrol for stopping seizure detection! error msg:' + e.toString());
        
        update_seizure_detection_checkbox();
    }
}


function update_checkboxes() {
    //update logging and alarm checkbox
    console.log('updating automatic_analysis checkbox');
    automatic_analysis_checkbox.checked = params.alarm;
}


//update seizure detection checkbox + make sure that settings are enabled or disabled accordingly!
function update_seizure_detection_checkbox() {	
    //update checkbox indicating whether service app is running:
    console.log('ask service app if the sensor listener is running...');
    var obj = new tizen.ApplicationControlData('service_action', ['running?']);
    var obj1 = new tizen.ApplicationControl('http://tizen.org/appcontrol/operation/service',
        null,
        null,
        null, [obj], 'SINGLE'
    );
    var appControlReplyCallback = {
        // callee sent a reply
        onsuccess: function(data) {
            //update checkbox!
            start_stop_checkbox.checked = (data[0].value[0] === '1');
            console.log('received: ' + data[0].value[0]);
            console.log('updated checkbox! (to ' + (data[0].value[0] === '1') + ')');
        },
        // callee returned failure
        onfailure: function() {
            console.log('reply failed');
            
            //change from loading page to mainpage
            tau.changePage('#main');
            //create warning for user!
            tau.openPopup('#RunningFailedPopup');
        }
    };
    try {
        tizen.application.launchAppControl(obj1,
            SERVICE_APP_ID,
            function() {
                console.log('Checking whether sensor listener is running succeeded');
            },
            function(e) {
                console.log('Checking whether sensor listener is running failed : ' + e.message);
            },
            appControlReplyCallback);
    } catch (e) {
        window.alert('Error when starting appcontrol to detect whether seizure detection is running! error msg:' + e.toString());
    }
}



function toggle_analysis() {
    if (start_stop_checkbox.checked) {
        start_service_app();
    } else {
        stop_service_app();
    }
}


function add_alarms() {
	console.log('add_alarm: start.');
	remove_alarms();
	console.log('add_alarm: alarms removed.');

	//generate appcontrol to be called on remove_alarm
    var obj_stop = new tizen.ApplicationControlData('service_action', ['stop']); //you'll find the app id in config.xml file.
    var appcontrol_stop = new tizen.ApplicationControl('http://tizen.org/appcontrol/operation/service',
        null,
        null,
        null, [obj_stop], 'SINGLE'
    );
   
    var obj_start = new tizen.ApplicationControlData('service_action', ['start']);
    var appcontrol_start = new tizen.ApplicationControl('http://tizen.org/appcontrol/operation/service',
        null,
        null,
        null, [obj_start], 'SINGLE'
    );
	console.log('add_alarm: created application controls.');
	
	//var notification_content_stop = {content: 'Automatically started seizure detection!', actions: {vibration: true}};
	//var notification_stop = new tizen.UserNotification('SIMPLE', 'Epilarm', notification_content_stop);

	//var notification_content_start ={content: 'Automatically stopped seizure detection!', actions: {vibration: true}};
	//var notification_start = new tizen.UserNotification('SIMPLE', 'Epilarm', notification_content_start);
	//console.log('add_alarm: created notifications.');
	
	//adapt alarm date days, s.t. they are in the future (but at the next occurance of the specified time!)
	//change year, month, date to today's
	const today = new Date()
	params.alarm_start_date.setFullYear(today.getFullYear());
	params.alarm_start_date.setMonth(today.getMonth());
	params.alarm_start_date.setDate(today.getDate());

	params.alarm_stop_date.setFullYear(today.getFullYear());
	params.alarm_stop_date.setMonth(today.getMonth());
	params.alarm_stop_date.setDate(today.getDate());
	
	//in case these dates have already passed, change year, month, date to tomorrow's
	const tomorrow = new Date(today)	
	tomorrow.setDate(tomorrow.getDate() + 1)

	if (params.alarm_start_date < today) {
		params.alarm_start_date.setFullYear(tomorrow.getFullYear());
		params.alarm_start_date.setMonth(tomorrow.getMonth());
		params.alarm_start_date.setDate(tomorrow.getDate());
	}
	if (params.alarm_stop_date < today) {
		params.alarm_stop_date.setFullYear(tomorrow.getFullYear());
		params.alarm_stop_date.setMonth(tomorrow.getMonth());
		params.alarm_stop_date.setDate(tomorrow.getDate());
	}
	//params's alarm dates are now set to the next occuring time in the future!

	//var alarm_notification_start = new tizen.AlarmAbsolute(params.alarm_start_date, params.alarm_days); //TODO do we additionally need notifications?
	//var alarm_notification_stop = new tizen.AlarmAbsolute(params.alarm_stop_date, params.alarm_days);
	var alarm_start = new tizen.AlarmAbsolute(params.alarm_start_date);//, params.alarm_days);
	var alarm_stop = new tizen.AlarmAbsolute(params.alarm_stop_date);//, params.alarm_days);
	console.log('add_alarm: created alarms.');
	
	//tizen.alarm.addAlarmNotification(alarm_notification_start, notification_start);
	//tizen.alarm.addAlarmNotification(alarm_notification_stop, notification_stop);
	//console.log('add_alarm: scheduled notifications.');

	tizen.alarm.add(alarm_stop, APP_ID, appcontrol_stop);
	console.log('add_alarm: scheduled stop appcontrol.');
	tizen.alarm.add(alarm_start, APP_ID, appcontrol_start);
	console.log('add_alarm: scheduled start appcontrol.');

	console.log('add_alarm: scheduled appcontrols.');

	console.log('add_alarm: done.');
}


//removes all scheduled alarms of app (i.e. all scheduled starting and stopping calls to service app)
function remove_alarms() {
	console.log('remove_alarm: start.');
	tizen.alarm.removeAll();
	console.log('remove_alarm: removed all scheduled alarms.');
	console.log('remove_alarm: done.');
}


function toggle_alarm() {
    if (automatic_analysis_checkbox.checked) {
        params.alarm = true;
        add_alarms();
    } else {
        params.alarm = false;
        remove_alarms();
    }
    save_params();
}


//re-launch UI and start seizure detection
function test_start() {
    var obj_start = new tizen.ApplicationControlData('service_action', ['start']);
    var appcontrol_start = new tizen.ApplicationControl('http://tizen.org/appcontrol/operation/service',
        null,
        null,
        null, [obj_start],
        'SINGLE'
    );
    tizen.application.launchAppControl(appcontrol_start,
            APP_ID,
            function() {
                console.log('Starting appcontrol succeeded.');
            },
            function(e) {
                console.log('Starting appcontrol failed : ' + e.message);
            }, null);

}

//re-launch UI and stop seizure detection
function test_stop() {
    var obj_start = new tizen.ApplicationControlData('service_action', ['stop']);
    var appcontrol_start = new tizen.ApplicationControl('http://tizen.org/appcontrol/operation/service',
        null,
        null,
        null, [obj_start],
        'SINGLE'
    );
    tizen.application.launchAppControl(appcontrol_start,
            APP_ID,
            function() {
                console.log('Starting appcontrol succeeded.');
            },
            function(e) {
                console.log('Starting appcontrol failed : ' + e.message);
            }, null);

}


function addEventListeners() {
	var getURLcb = function(url) {
		return function(event) {
			if(!event.target.classList.contains("disabled")){
				console.log(event);
				tau.changePage(url);
			}
		}
	};
		
	settings_alarm_start.addEventListener("click", getURLcb("/contents/settings/automatic_start_picker.html"));

	settings_alarm_stop.addEventListener("click", getURLcb("contents/settings/automatic_stop_picker.html"));


	//checkboxes
    start_stop_checkbox.addEventListener('change', toggle_analysis);
	
    automatic_analysis_checkbox.addEventListener('change', toggle_alarm);
}


function loadListElements() {
	//define vars for all relevant list items and add eventlisteners!
    list = tau.widget.Listview(document.getElementById('main_list'))._items;
    
    //settings:    
    start_stop = list.find(el => el.id == 'start_stop');
    start_stop_checkbox = start_stop.childNodes[3].childNodes[1];
        
    settings_alarm_start = list.find(el => el.id == 'settings_alarm_start');
    settings_alarm_stop = list.find(el => el.id == 'settings_alarm_stop');
    
    automatic_analysis = list.find(el => el.id == 'automatic_analysis');
    automatic_analysis_checkbox = automatic_analysis.childNodes[3].childNodes[1];

}


window.onload = function() {
    //leave screen on
    //tizen.power.request('SCREEN', 'SCREEN_NORMAL');
    
	//initialize all list elems
	loadListElements();

	//load user settings (or default!)
    load_params();
	
    //update 'simple' checkboxes (where service app must not be asked for)
    update_checkboxes();
    
    //seizure detection checkbox is updated when handling appcontrol:
    
    //check whether app was launched due to some appControl:
    var reqAppControl = tizen.application.getCurrentApplication().getRequestedAppControl();
    if (reqAppControl) {
    	
    	if (reqAppControl.appControl.operation != 'http://tizen.org/appcontrol/operation/default') {
    		//launched by non-default appcontrol, e.g., not started from drawer.
    		console.log('launched by appcontrol!');
    	    console.log(reqAppControl);
    	}
    	
        if (reqAppControl.appControl.data.length>0 && reqAppControl.appControl.operation === 'http://tizen.org/appcontrol/operation/service') {
        	//called from an appcontrol with operation name 'service'
        	if (reqAppControl.appControl.data[0].key === 'service_action' && reqAppControl.appControl.data[0].value[0] === 'start') {
        		//appcontrol request to start service app, also open Popup informing user!
        		console.log('appcontrol with service action _start_ received!');
        		
        		start_service_app(); //updates seizure detection checkbox aufter launch!
        		//open popup (timeout needed as otherwise the popup is not correctly aligned on the watch's display!)
        		setTimeout(function(event){    				
        			tau.openPopup('#AutomaticDetectionStartPopup');
    			},100)
    		} else if (reqAppControl.appControl.data[0].key === 'service_action' && reqAppControl.appControl.data[0].value[0] === 'stop') {
        		//appcontrol request to stop service app, also open Popup informing user!
        		console.log('appcontrol with service action _stop_ received!');
        		
    			stop_service_app(); //updates seizure detection checkbox aufter termination!
        		//open popup (timeout needed as otherwise the popup is not correctly aligned on the watch's display!)
    			setTimeout(function(event){    				
    				tau.openPopup('#AutomaticDetectionStopPopup');
    			},100)
			}
        }
    } else {
        console.log('not launched by appcontrol.');
        
        update_seizure_detection_checkbox();
    }
    
    //finally load elements and addEventlisteners, this makes sure that the checkbox is (probably) already updated before the user can change it!
    setTimeout(function(event){
    	addEventListeners();
    },200);

    console.log('UI started!');
};


window.onpageshow = function() {
	update_checkboxes();
	update_seizure_detection_checkbox();
};