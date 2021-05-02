/*global tau */
(function () {
	var page, element, SAVED_TIME_KEY;
	var widget = null, savedTimeValue = null;
	
	//change correct values based on calling page
	if (document.getElementById("number-picker-alarm-start-page")) {
		page=document.getElementById("number-picker-alarm-start-page");
		SAVED_TIME_KEY = "alarm_start_date";
	} else if (document.getElementById("number-picker-alarm-stop-page")) {
		page=document.getElementById("number-picker-alarm-stop-page");
		SAVED_TIME_KEY = "alarm_stop_date";
	}
	
	element = page.querySelector(".ui-time-picker")
	
	function init() {
		widget = tau.widget.TimePicker(element);
		element.addEventListener("change", onChange);

		savedTimeValue = params[SAVED_TIME_KEY];
		if (savedTimeValue) {
			widget.value(new Date(savedTimeValue));
		}
	}

	function onChange(event) {
		console.log("time-picker-24: changed value of", SAVED_TIME_KEY, " to ", event.detail.value);
		params[SAVED_TIME_KEY] = event.detail.value;
	}

	function onPageHide() {
		element.removeEventListener("change", onChange);
		widget.destroy();
	}

	page.addEventListener("pagebeforeshow", init);
	page.addEventListener("pagehide", onPageHide);
}());
