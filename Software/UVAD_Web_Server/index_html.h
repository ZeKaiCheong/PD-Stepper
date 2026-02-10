const char index_html[] PROGMEM = R"rawliteral(




<!DOCTYPE HTML><html><head>
  <title>UVAD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  
  <style>
  
  body {
    font-family: "Lato", sans-serif;
    background-color: #232324; 
    padding: 15px;
    width: 460px;
    margin:0 auto;
    color: white;
  }
  
  #inputs {
    border-radius: 20px;
    padding: 20px;
    margin: 0 auto;
    background-color: #2e2e2e; 
    width: 400px;
  }
  
  hr {
    width: 350px;
  }
  
  h2 {
    text-align: center;
  }
  
  h1 {
    margin-top: 0px;
    margin-bottom: 15px;
    font-size: 40px;
    text-align: center;
    text-decoration: ;
    color: #fc4903;
    font-family: Tahoma, sans-serif;
    font-weight: normal;
  }
  
  #middle-box {
    width: 320px;
    margin-left: auto;
    margin-right: auto;
    margin-bottom: 20px;
  }
  
  .drop-box {
    float: right;
    height: 25px;
    padding-left: 5px;
    padding-right: 5px;
    color: white;
    background-color: #2b2b2b;
    border-radius: 5px;
    border-style: solid;
    border-color: #919191;
    border-width: 1px;
  }
  
  
  #bottom-box {
    text-align: center;
  }
  
  .save-button {
    background-color: #fc4903;
    border: none;
    color: white;
    padding: 14px 30px;
    text-align: center;
    text-decoration: none;
    display: inline-block;
    font-size: 16px;
    margin: 4px 2px;
    transition-duration: 0.4s;
    cursor: pointer;
    border-radius: 10px;
    font-size: 17px;
  }
  
    .pos-button {
    background-color: #fc4903;
    border: none;
    color: white;
    padding: 10px 14px;
    text-align: center;
    text-decoration: none;
    display: inline-block;
    font-size: 16px;
    margin: 4px 2px;
    transition-duration: 0.4s;
    cursor: pointer;
    border-radius: 10px;
    
  }

  
  
  /* ----- CHECKBOX ------- */
  .container {
    display: inline;
    position: relative;
    padding-left: 35px;
    margin-bottom: 12px;
    margin-left: 160px;
    cursor: pointer;
    font-size: 22px;
    -webkit-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
  }
  #enabled1-text, #enabled2-text {
      padding-top:5px;
      display: inline-block;
      margin-right: 20px;
  }
  /* Hide the browser's default checkbox */
  .container input {
    position: absolute;
    opacity: 0;
    cursor: pointer;
    height: 0;
    width: 0;
  }

  /* Create a custom checkbox */
  .checkmark {
    position: absolute;
    top: 0;
    left: 0;
    height: 25px;
    width: 25px;
    background-color: #eee;
  }

  /* On mouse-over, add a grey background color */
  .container:hover input ~ .checkmark {
    background-color: #ccc;
  }

  /* When the checkbox is checked, add a blue background */
  .container input:checked ~ .checkmark {
    background-color: #fc4903;
  }

  /* Create the checkmark/indicator (hidden when not checked) */
  .checkmark:after {
    content: "";
    position: absolute;
    display: none;
  }

  /* Show the checkmark when checked */
  .container input:checked ~ .checkmark:after {
    display: block;
  }

  /* Style the checkmark/indicator */
  .container .checkmark:after {
    left: 9px;
    top: 5px;
    width: 5px;
    height: 10px;
    border: solid white;
    border-width: 0 3px 3px 0;
    -webkit-transform: rotate(45deg);
    -ms-transform: rotate(45deg);
    transform: rotate(45deg);
  }

  #powergood, #position, #status, #voltage, #stallguard{
    float: right;
  }

  .stat-ok {
    color: #2ecc71;
    font-weight: bold;
  }

  .stat-warn {
    color: #f1c40f;
    font-weight: bold;
  }

  .stat-error {
    color: #e74c3c;
    font-weight: bold;
  }

  .mini-button {
    float: right;
    background-color: #3b3b3b;
    border: 1px solid #5c5c5c;
    color: white;
    padding: 4px 8px;
    border-radius: 6px;
    font-size: 12px;
    cursor: pointer;
    margin-left: 8px;
  }

  .stop-button {
    background-color: #d32f2f;
    border: none;
    color: white;
    padding: 14px 30px;
    text-align: center;
    text-decoration: none;
    display: inline-block;
    font-size: 17px;
    margin: 4px 2px;
    transition-duration: 0.4s;
    cursor: pointer;
    border-radius: 10px;
  }
  
    .slider1 {
      width: 320px;
      height: 40px;
      accent-color: #fc4903;
   }

    .slider-direction {
      display: flex;
      justify-content: space-between;
      width: 320px;
      font-size: 30px;
      line-height: 0;
    }


    /* Style the toggle switch container */
    .toggle-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      margin-bottom: 20px; /* Adjust spacing between checkbox and slider */
    }

    /* Style the toggle switch */
    .toggle-switch {
      float: right;
      position: relative;
      display: inline-block;
      width: 50px;
      height: 24px;
    }

    /* Hide the default checkbox */
    .toggle-switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }

    /* The slider */
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      -webkit-transition: .4s;
      transition: .4s;
      border-radius: 34px;
    }

    /* Rounded sliders */
    .slider.round {
      border-radius: 24px;
    }

    /* Slider styling when toggled */
    .slider:before {
      position: absolute;
      content: "";
      height: 16px;
      width: 16px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      -webkit-transition: .4s;
      transition: .4s;
      border-radius: 50%;
    }

    input:checked + .slider {
      background-color: #fc4903;
    }

    input:focus + .slider {
      box-shadow: 0 0 1px #2196F3;
    }

    input:checked + .slider:before {
      -webkit-transform: translateX(26px);
      -ms-transform: translateX(26px);
      transform: translateX(26px);
    }
    
    img{
      width: 250px;
        display: block;
        margin-left: auto;
        margin-right: auto;
        margin-top: 20px;
    }

  
  </style>
  
</head><body>

  <div id="inputs">

    <h1>UVAD</h1>
  <hr>
    <form action="/get">
      <h2>Live Stats</h2>
      <div id="middle-box">
        <span class="stat-labels">PD Voltage:</span>
        <span id="voltage">Loading</span>
        <br>
        <br>
        <span class="stat-labels">USB PD Status:</span>
        <span id="powergood">Loading</span>
        <br>
        <br>
        <span class="stat-labels">Driver Status:</span>
        <span id="status">Loading</span>
        <br>
        <br>
        <span class="stat-labels">Stall Status:</span>
        <span id="stallguard">Loading</span>
        <br>
        <br>
        <span class="stat-labels">Output Angle:</span>
        <button class="mini-button" type="button" onclick="zeroOutputAngle()">Zero</button>
        <span id="position">Loading</span>
      </div>
    </form>
 
    <hr>
  
    <!--
    <form action="/save" method="post">
    <h2>Settings</h2>
      <div id="middle-box">
        <label for="enabled1" id="enabled1-text">Driver Enable:</label>
        <label class="container">
          <input %enabled1% type="checkbox" id="enabled1" name="enabled1" value="Enabled">
          <span class="checkmark"></span>
        </label>
        
        <br>
        <br>
        
        <label for="setvoltage">Voltage:</label>
        <select id="setvoltage" name="setvoltage" class="drop-box">
          <option value="5">5V</option>
          <option value="9">9V</option>
          <option value="12">12V</option>
          <option value="15">15V</option>
          <option value="20">20V</option>
        </select>
        <br>
        <br>

        <label for="microsteps">Microsteps:</label>
        <select id="microsteps" name="microsteps" class="drop-box">
          <option value="1">1</option>
          <option value="4">4</option>
          <option value="8">8</option>
          <option value="16">16</option>
          <option value="32">32</option>
          <option value="64">64</option>
          <option value="128">128</option>
          <option value="256">256</option>
        </select>
        <br>
        <br>

        <label for="current">Max Current:</label>
        <select id="current" name="current" class="drop-box">
          <option value="10">10&#37;</option>
          <option value="20">20&#37;</option>
          <option value="30">30&#37;</option>
          <option value="40">40&#37;</option>
          <option value="50">50&#37;</option>
          <option value="60">60&#37;</option>
          <option value="70">70&#37;</option>
          <option value="80">80&#37;</option>
          <option value="90">90&#37;</option>
          <option value="100">100&#37;</option>
        </select>
        <br>
        <br>

      <label for="stall_threshold">Stall Threshold:</label>
        <select id="stall_threshold" name="stall_threshold" class="drop-box">
          <option value="5">5</option>
          <option value="10">10</option>
          <option value="20">20</option>
          <option value="30">30</option>
          <option value="40">40</option>
          <option value="50">50</option>
          <option value="60">60</option>
          <option value="70">70</option>
          <option value="80">80</option>
          <option value="90">90</option>
          <option value="100">100</option>
          <option value="110">110</option>
          <option value="120">120</option>
          <option value="130">130</option>
          <option value="140">140</option>
          <option value="150">150</option>
        </select>
        <br>
        <br>
        <label for="standstill_mode">Standstill mode:</label>
        <select id="standstill_mode" name="standstill_mode" class="drop-box">
          <option value="NORMAL">Normal</option>
          <option value="FREEWHEELING">Freewheeling</option>
          <option value="BRAKING">Braking</option>
          <option value="STRONG_BRAKING">Strong-Braking</option>
        </select>
        
      </div>

      <div id="bottom-box">
        <input class="save-button" type="submit" value="Save">
      </div>
    </form>
    
    
    
    <br>
    <hr>
    -->
    
    <h2>Velocity Control</h2>
    <div id="middle-box">
      <!-- Snap Centre toggle hidden; default remains enabled. -->
      <!--
      <label for="return">Snap Centre:</label>
      <label class="toggle-switch">
        <input type="checkbox" id="toggleReset" onclick="toggleReset()" checked>
        <span class="slider round"></span>
      </label>
      <br>
      -->
      <div class="slider-direction"><span>-</span><span>+</span></div>
      <input type='range' class='slider1' id='slider' min='-320' max='320' value='0' oninput='throttledUpdate()' onpointerup='checkReset()'>
   </div>   
   
   <hr>
    
   <h2>Position Control</h2>
   <div id="bottom-box">
    <button class="pos-button" type="submit" onclick="but1()">-%large_angle%&deg;</button>
    <button class="pos-button" type="submit" onclick="but2()">-%small_angle%&deg;</button>
    <button class="pos-button" type="submit" onclick="but3()">+%small_angle%&deg;</button>
    <button class="pos-button" type="submit" onclick="but4()">+%large_angle%&deg;</button>
   </div>
   
    <br>
  <hr>


   <div id="bottom-box">
     <button class="stop-button" type="button" onclick="stopMotion()">STOP</button>
   </div>
   <br>

   <hr>

     
  </div>
  
  
</body></html>

<script>
    /* Auto fill dropdowns with saved values */ 
    
    var temp = "%microsteps%";
    var mySelect = document.getElementById('microsteps');
    if (mySelect) {
      for (var i, j = 0; i = mySelect.options[j]; j++) {
        if (i.value == temp) {
          mySelect.selectedIndex = j;
          break;
        }
      }
    }
  
    var temp = "%voltage%";
    var mySelect = document.getElementById('setvoltage');
    if (mySelect) {
      for (var i, j = 0; i = mySelect.options[j]; j++) {
        if (i.value == temp) {
          mySelect.selectedIndex = j;
          break;
        }
      }
    }
  
    var temp = "%current%";
    var mySelect = document.getElementById('current');
    if (mySelect) {
      for (var i, j = 0; i = mySelect.options[j]; j++) {
        if (i.value == temp) {
          mySelect.selectedIndex = j;
          break;
        }
      }
    }
  
    var temp = "%stall_threshold%";
    var mySelect = document.getElementById('stall_threshold');
    if (mySelect) {
      for (var i, j = 0; i = mySelect.options[j]; j++) {
        if (i.value == temp) {
          mySelect.selectedIndex = j;
          break;
        }
      }
    }
  
    var temp = "%standstill_mode%";
    var mySelect = document.getElementById('standstill_mode');
    if (mySelect) {
      for (var i, j = 0; i = mySelect.options[j]; j++) {
        if (i.value == temp) {
          mySelect.selectedIndex = j;
          break;
        }
      }
    }

  
  /* Auto update stats without refresh */ 
  function applyStatusClass(element, text) {
    var value = text.trim();
    element.classList.remove("stat-ok", "stat-warn", "stat-error");

    if (value === "Power Good" || value === "No Errors" || value === "Not Stalled") {
      element.classList.add("stat-ok");
    } else if (value === "Over Temp Warning") {
      element.classList.add("stat-warn");
    } else if (value === "Over Temp Shutdown" || value === "Power Bad" || value === "Stalled") {
      element.classList.add("stat-error");
    }
  }

  function setTextValue(elementId, text, colorize) {
    var el = document.getElementById(elementId);
    el.innerHTML = text;
    if (colorize) {
      applyStatusClass(el, text);
    }
  }

  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        setTextValue("voltage", this.responseText, false);
      }
    };
    xhttp.open("GET", "/voltage", true);
    xhttp.send();
  }, 390 ) ;
  
  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        setTextValue("powergood", this.responseText, true);
      }
    };
    xhttp.open("GET", "/powergood", true);
    xhttp.send();
  }, 410 ) ;
  
  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        setTextValue("status", this.responseText, true);
      }
    };
    xhttp.open("GET", "/status", true);
    xhttp.send();
  }, 300 ) ;
  
  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        setTextValue("position", this.responseText, false);
      }
    };
    xhttp.open("GET", "/position", true);
    xhttp.send();
  }, 130 ) ;
  
  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        setTextValue("stallguard", this.responseText, true);
      }
    };
    xhttp.open("GET", "/stallguard", true);
    xhttp.send();
  }, 150 ) ;


  /* Throttle sending rate */
  let lastCall = 0;
  const throttleTimeout = 100; 
  
  function throttledUpdate() {
    const now = new Date().getTime();
    if (now - lastCall < throttleTimeout) return; // Skip if too soon
    lastCall = now;
    updateSlider();
  }
    
  
  /* Scripts for velocity toggle and slider */
  var resetEnabled = true;

  function updateSlider() {
    var sliderValue = document.getElementById('slider').value;
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('slider=' + sliderValue);
  }

  function toggleReset() {
    var toggleCheckbox = document.getElementById('toggleReset');

    if (toggleCheckbox.checked) {
      // Reset slider to middle position if toggle is checked
      resetSlider();
    }

    resetEnabled = toggleCheckbox.checked;
  }

  function checkReset() {
    if (resetEnabled) {
      resetSlider(); // Reset slider if enabled
    }
  }

  function resetSlider() {
    document.getElementById('slider').value = 0; // Reset to middle position
    updateSlider(); // Trigger update
  }
  
  /* handle button presses */
  function but1() {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('positionControl=1');
  }
  
  function but2() {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('positionControl=2');
  }
  
  function but3() {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('positionControl=3');
  }
  
  function but4() {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('positionControl=4');
  }

  function zeroOutputAngle() {
    if (!confirm("Reset Output Angle to 0?")) {
      return;
    }
    stopMotion(); // Stop motion before zeroing
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/zero_angle', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('zero=1');
    setTextValue("position", "0°", false);
  }

  function stopMotion() {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/stop', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('stop=1');
    document.getElementById('slider').value = 0;
  }
  
</script>





)rawliteral";
