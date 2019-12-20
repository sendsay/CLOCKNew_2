var xmlHttp = new XMLHttpRequest();

function getData() {
  if(xmlHttp.readyState==0 || xmlHttp.readyState==4){
  xmlHttp.open('GET','/getData',true);
  xmlHttp.send();
  xmlHttp.onload = function(e) {

      var str = xmlHttp.responseText;

      json=JSON.parse(str);
      //wifi
      document.getElementById("ssid").value = json.ssid;
      document.getElementById("password").value = json.password;
      document.getElementById("ssidAP").value = json.ssidAP;
      document.getElementById("passwordAP").value = json.passwordAP;
      //time
      document.getElementById("timezone").value = json.timezone;
      if (json.summertime == 1) { document.getElementById("summerTime").checked = true; }
      document.getElementById("sigOn").value = json.sigOn;
      document.getElementById("sigOff").value = json.sigOff;
      document.getElementById("ntpServerName").value = json.ntpServerName;
      //weather
      document.getElementById("apiKey").value = json.apiKey;
      document.getElementById("cityId").value = json.cityId;
      document.getElementById("weatherServer").value = json.weatherServer;
      document.getElementById("langWeather").value = json.langWeather;
      //mqtt
      document.getElementById("mqttserver").value = json.mqttserver;
      document.getElementById("mqttport").value = json.mqttport;
      document.getElementById("mqttUserName").value = json.mqttUserName;
      document.getElementById("mqttpass").value = json.mqttpass;
      document.getElementById("mqttname").value = json.mqttname;
      document.getElementById("mqttsubinform").value = json.mqttsubinform;
      document.getElementById("mqttsub").value = json.mqttsub;
      document.getElementById("mqttpubtemp").value = json.mqttpubtemp;
      document.getElementById("mqttpubtempUl").value = json.mqttpubtempUl;
      document.getElementById("mqttpubhum").value = json.mqttpubhum;
      document.getElementById("mqttpubpress").value = json.mqttpubpress;
      // document.getElementById("mqttpubalt").value = json.mqttpubalt;
      document.getElementById("mqttpubforecast").value = json.mqttpubforecast;
      document.getElementById("mqttbutt").value = json.mqttbutt;

      if (json.mqttOn == 1) {
        document.getElementById("mqttOn1").checked = true;
        changeContent(true);
      }
      if (json.weatherOn == 1) {
        document.getElementById("weatherOn").checked = true;
        changeContentWeather(true);
      }
      if (json.autoBright == 1) {
        document.getElementById("autoBright").checked = true;
        changeContentautoBright(true);
      }
    }
  }
}

function saveButton() {
  var content = "/saveContent?ssid=" + val('ssid') + "&password=" + val('password') + "&ssidAP=" + val('ssidAP') +
                "&passwordAP=" + val('passwordAP') + "&timezone=" + val('timezone') + "&summertime=" + val_sw('summerTime') +
                "&sigOn=" + val('sigOn') + "&sigOff=" + val('sigOff') + "&ntpServerName=" + val('ntpServerName') +
                "&apiKey=" + val('apiKey') + "&cityId=" + val('cityId') + "&weatherServer=" + val('weatherServer') +
                "&langWeather=" + val("langWeather") + "&mqttserver=" + val("mqttserver") + "&mqttport=" + val('mqttport') +
                "&mqttUserName=" + val('mqttUserName') + "&mqttpass=" + val('mqttpass') + "&mqttname=" + val('mqttname') +
                "&mqttsubinform=" + val('mqttsubinform') + "&mqttsub=" + val('mqttsub') + "&mqttpubtemp=" + val('mqttpubtemp') +
                "&mqttpubtempUl=" + val('mqttpubtempUl') + "&mqttpubhum=" + val('mqttpubhum') +
                "&mqttpubpress=" + val('mqttpubpress') + "&mqttpubforecast=" + val('mqttpubforecast') +
                "&mqttbutt=" + val('mqttbutt') + "&mqttOn=" + val_sw('mqttOn1') + "&weatherOn=" + val_sw('weatherOn') +
                "&autoBright=" + val_sw('autoBright');

  console.log("************* send to server ");
  console.log(content);

  xmlHttp.open('GET', content,true);
  xmlHttp.send();
}

function restartButton() {
  xmlHttp.open("GET", "/restart", true);
  xmlHttp.send();
}

function val(id){
  var v = document.getElementById(id).value;
  return v;
}

function val_sw(nameSwitch) {
  var switchOn = document.getElementById(nameSwitch);
  if (switchOn.checked){
    return 1;
  }
  return 0;
}

function changeContent(changeMqttOn) {
  // var changeMqttOn = document.getElementById("mqttOn1").checked;

  var mqttsection = document.getElementById("mqtt");
  var mqttinput = mqttsection.getElementsByTagName("input");

  for (let i= 0; i < 13; i++) {
    mqttinput[i].disabled = !changeMqttOn;
  }
}

function changeContentWeather(changeWeatherOn) {
  // var changeWeatherOn = document.getElementById("weatherOn").checked;

  var weathersection = document.getElementById("weather");
  var weatherinput = weathersection.getElementsByTagName("input");

  for (let i= 0; i < 3; i++) {
      weatherinput[i].disabled = !changeWeatherOn;
  }
  document.getElementById("langWeather").disabled = !changeWeatherOn;
}

function changeContentautoBright(changeAutoBright) {
  // var changeAutoBright = document.getElementById("autoBright").checked;

  document.getElementById("manualBright").disabled = changeAutoBright;

}
