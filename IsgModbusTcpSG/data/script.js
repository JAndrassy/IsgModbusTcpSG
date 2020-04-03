
var host = location.hostname;
var restPort = 80;
if (host == "") {
  host = "192.168.1.110";
}

function onLoad(cmd) {
  var xhr = new XMLHttpRequest();
  xhr.onerror = function(e) {
    alert(xhr.status + " " + xhr.statusText);
  }
  xhr.onload = function(e) {
    if (cmd == "E") {
      showEvents(xhr.responseText);
    } else if (cmd == "L") {
      showCsvFilesList(xhr.responseText);
    } else {
      showValues(xhr.responseText);
    }
  };
  xhr.open("GET", "http://" + host + ":" + restPort + "/" + cmd, true);
  xhr.send();
}

var valueLabels = {"es" : "Error state", "err" : "Errors", "mode" : "Mode", "i" : "Input pin", "o" : "SG INPUT1", "op" : "Op.state", "w" : "Waiting", "ec" : "Events", "csv" : "CSV Files", "v" : "Version"};
var modeLabels = {"A" : "AUTOMATIC", "M" : "MANUAL"};
var errorLabels = {" " : "-", "N" : "network", "M" : "modbus", "O" : "op.state"};

function showValues(jsonData) {
  var data = JSON.parse(jsonData);
  var contentDiv = document.getElementById("contentDiv");
  while(contentDiv.firstChild){
    contentDiv.removeChild(contentDiv.firstChild);
  } 
  contentDiv.appendChild(createCommandBox("Data", "Refresh", "I"));
  if (data["mode"] == 'M') {
    contentDiv.appendChild(createCommandBox("Automatic", "START", "A"));
  }
  var waiting = (data["w"] != null);
  for (var key in valueLabels) {
    var val = data[key];
    if (val == null)
      continue;
    var unit = "";
    if (key == "mode") {
      val = modeLabels[val];
    } else if (key == "es") {
      val = errorLabels[val];
    } else if (key == "w") {
      unit = "sec";
    }
    var boxDiv = document.createElement("DIV");
    if (key == "ec" || key == "err" || key == "csv") {
      boxDiv.className = "value-box value-box-clickable";
    } else {
      boxDiv.className = "value-box";
    }
    boxDiv.appendChild(createTextDiv("value-label", valueLabels[key]));
    var classes = "value-value";
    if (key == "op" && waiting) {
      classes += " value-value-blinking"
    }
    boxDiv.appendChild(createTextDiv(classes, val + unit));
    if (key == 'ec' || key == 'err') {
      boxDiv.onclick = function() {
        location = "events.html";
      }
    } else if (key == "csv") {
      boxDiv.onclick = function() {
        location = "csvlst.html";
      }
   }
    contentDiv.appendChild(boxDiv);
    if (key == "op" && !waiting) {
      contentDiv.appendChild(createCommandBox("ISG SG", "Query", "Q"));
    }
  }
  var box;
  if (data["o"] == '1') {
    box = createCommandBox("SG INPUT1", "Set Off", "0");
  } else {
    box = createCommandBox("SG INPUT1", "Set On", "1");
  }
  var state = data["mode"];
  if (state == 'A') {
    contentDiv.appendChild(box);
  } else {
    contentDiv.insertBefore(box, contentDiv.childNodes[1]);
  }
  if (waiting) {
    setTimeout(function() { onLoad('I')}, 5000);
  }
}

var eventHeaders = ["timestamp", "event", "value 1", "value 2", "count"];
var eventLabels = ["Events", "Reset", "Watchdog", "Network", "Modbus", "Manual run", "OS wait"];
var eventIsError = [false, false, true, true, true, false, true];

function showEvents(jsonData) {
  var data = JSON.parse(jsonData);
  var contentDiv = document.getElementById("contentDiv");
  var eventsHeaderDiv = document.createElement("DIV");
  eventsHeaderDiv.className = "table-header";
  for (var i in eventHeaders) {
    eventsHeaderDiv.appendChild(createTextDiv("table-header-cell", eventHeaders[i]));
  }
  contentDiv.appendChild(eventsHeaderDiv);
  var events = data.e;
  events.sort(function(e1, e2) { return (e2.t - e1.t); });
  var today = new Date();
  today.setHours(0,0,0,0);
  for (var i = 0; i < events.length; i++) {
    var event = events[i];
    var tmpstmp = "";
    if (event.t != 0) {
      tmpstmp = t2s(event.t);
    }
    var v1 = "";
    if (event.v1 != 0) {
      v1 = "" + event.v1;
    }
    var v2 = "";
    if (event.v2 != 0) {
      v2 = event.v2;
    }
    var eventDiv = document.createElement("DIV");
    var date = new Date(event.t * 1000);
    if (today < date && event.i < eventIsError.length && eventIsError[event.i]) {
      eventDiv.className = "table-row table-row-error";
    } else {
      eventDiv.className = "table-row";
    }
    eventDiv.appendChild(createTextDiv("table-cell", tmpstmp));
    eventDiv.appendChild(createTextDiv("table-cell", eventLabels[event.i]));
    eventDiv.appendChild(createTextDiv("table-cell table-cell-number", v1));
    eventDiv.appendChild(createTextDiv("table-cell table-cell-number", v2));
    eventDiv.appendChild(createTextDiv("table-cell table-cell-number", "" + event.c));
    contentDiv.appendChild(eventDiv);
  }
  contentDiv.appendChild(createButton("Log", "EVENTS.LOG"));
}

function showCsvFilesList(jsonData) {
  var data = JSON.parse(jsonData);
  var contentDiv = document.getElementById("contentDiv");
  var eventsHeaderDiv = document.createElement("DIV");
  eventsHeaderDiv.className = "table-header";
  eventsHeaderDiv.appendChild(createTextDiv("table-header-cell", "File"));
  eventsHeaderDiv.appendChild(createTextDiv("table-header-cell", "Size (bytes)"));
  contentDiv.appendChild(eventsHeaderDiv);
  var files = data.f;
  files.sort(function(f1, f2) { return -f1.fn.localeCompare(f2.fn); });
  for (var i = 0; i < files.length; i++) {
    var file = files[i];
    var fileDiv = document.createElement("DIV");
    fileDiv.className = "table-row";
    fileDiv.appendChild(createLinkDiv("table-cell", file.fn, "/CSV/" + file.fn));
    fileDiv.appendChild(createTextDiv("table-cell table-cell-number", "" + file.size));
    contentDiv.appendChild(fileDiv);
  }
}

function createTextDiv(className, value) {
  var div = document.createElement("DIV");
  div.className = className;
  div.appendChild(document.createTextNode("" + value));
  return div;
}

function createLinkDiv(className, value, url) {
  var div = document.createElement("DIV");
  div.className = className;
  var link = document.createElement("A");
  link.href = url;
  link.appendChild(document.createTextNode("" + value));
  div.appendChild(link);
  return div;
}

function createButton(text, command) {
  var button = document.createElement("BUTTON");
  button.className = "button";
  button.onclick = function() {
    if (command.length > 1) {
      location = command;
    } else {
      if (command != 'Q' && command != 'I' && !confirm("Are you sure?"))
        return;
      onLoad(command);
    }
  }
  button.appendChild(document.createTextNode(text));
  return button;
}

function createCommandBox(title, label, command) {
  var boxDiv = document.createElement("DIV");
  boxDiv.className = "value-box";
  boxDiv.appendChild(createTextDiv("value-label", title));
  var div = document.createElement("DIV");
  div.className = "value-value";
  div.appendChild(createButton(label, command));
  boxDiv.appendChild(div);
  return boxDiv;
}

function t2s(t) {
  var date = new Date(t * 1000);
  var tmpstmp = date.toISOString();
  return tmpstmp.substring(0, tmpstmp.indexOf('.')).replace('T', ' ');
}
