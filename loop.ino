void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  // Initialize gamepad struct
  for (int i = 0; i < PAGES; i++) {
    gp[i].x = 0;
    gp[i].y = 0;
    gp[i].buttons = 0;
  }

  Serial.begin(115200);
  usb_hid.begin();

  // wait until device mounted
  while (!TinyUSBDevice.mounted()) delay(1);

  // Display
  Wire.setSDA(SDA);
  Wire.setSCL(SCL);
  Wire.begin();
  u8g2.setI2CAddress(SSD1315_ADDR);
  u8g2.begin();

  // Initialize row pins as outputs
  for (int i = 0; i < 3; i++) {
    pinMode(rows[i], OUTPUT);
    digitalWrite(rows[i], LOW);
  }

  // Initialize column pins as inputs with pullup resistors
  for (int j = 0; j < 8; j++) {
    pinMode(cols[j], INPUT_PULLUP);
  }

  // Rotary Buttons
  pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);
  e1.begin();
  e2.begin();

  // EEPROM config size is calculated by PAGE Size
  EEPROM.begin(PAGES * 128);
  loadLabels();
}

void loop() {
  handleSerial();
  handleCurrentPage();
  handleDisplay();
  handleSliders();
  handleButtons();

  usb_hid.sendReport(currentGamepad + 1, &gp[currentGamepad], sizeof(gp[currentGamepad]));
}


void handleSerial() {
  static String inputString = "";  // a string to hold incoming data
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      parseSerialInput(inputString);
      inputString = "";
    } else {
      inputString += inChar;
    }
  }
}

// Parses serial Input [PAGE];[LABELNUM];[STRING] (0;0;WPN) or SAVE
void parseSerialInput(String input) {
  // Save endpoint
  if (input == "SAVE") {
    saveLabels();
    return;
  } 

  int page, labelIndex;
  // 3 chars + null terminator
  char label[4] = { 0 }; 

  int firstSemi = input.indexOf(';');
  int secondSemi = input.indexOf(';', firstSemi + 1);

  if (firstSemi > 0 && secondSemi > firstSemi) {
    page = input.substring(0, firstSemi).toInt();
    labelIndex = input.substring(firstSemi + 1, secondSemi).toInt();
    input.substring(secondSemi + 1).toCharArray(label, 4);

    if (page >= 0 && page < PAGES && labelIndex >= 0 && labelIndex < 13) {
      strncpy(labels[page][labelIndex], label, 4);
    }
  }
}

void handleCurrentPage() {
  if (!digitalRead(BUTTON_LEFT_PIN)) {
    ButtonLeft = true;
  } else {
    if (ButtonLeft == true) {
      currentGamepad -= 1;
    }
    ButtonLeft = false;
  }

  if (!digitalRead(BUTTON_RIGHT_PIN)) {
    ButtonRight = true;
  } else {
    if (ButtonRight == true) {
      currentGamepad += 1;
    }
    ButtonRight = false;
  }

  // Page trough
  if (currentGamepad < 0) {
    currentGamepad = PAGES - 1;
  } else if (currentGamepad > PAGES - 1) {
    currentGamepad = 0;
  }
}

void updateDisplay(int page) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // Print the Labels
  for (int i = 0; i < 12; i++) {
    int x = i % 4;
    int y = i / 4 + 1;

    // Offset is calculated here
    u8g2.setCursor(x * 33, y * 15);
    u8g2.print(labels[page][i]);
  }

  u8g2.drawHLine(0, 50, 128);

  // Page label (the 13 item in the array)
  u8g2.setCursor(75, 64);
  u8g2.print(labels[page][12]);

  // Rotary 1 Position
  String out = String(gp[page].x);
  u8g2.drawStr(0, 64, out.c_str());

  // Rotary 2 Position
  String out2 = String(gp[page].y);
  u8g2.drawStr(37, 64, out2.c_str());

  // Print current Page
  String out3 = String(page + 1);
  u8g2.drawStr(110, 64, out3.c_str());
  u8g2.drawStr(117, 64, "/4");

  u8g2.sendBuffer();
}

void handleDisplay() {
  updateDisplay(currentGamepad);
}

void handleSliders() {
  // TODO maybe buttons?
  int slide1diff = slider1_old - e1.getCount();
  int slide2diff = slider2_old - e2.getCount();
  slider1_old = e1.getCount();
  slider2_old = e2.getCount();

  slider[currentGamepad][0] += slide1diff;
  slider[currentGamepad][1] += slide2diff;

  if (slider[currentGamepad][0] <= -127) {
    slider[currentGamepad][0] = -127;
  }
  if (slider[currentGamepad][1] <= -127) {
    slider[currentGamepad][1] = -127;
  }

  if (slider[currentGamepad][0] >= 127) {
    slider[currentGamepad][0] = 127;
  }
  if (slider[currentGamepad][1] >= 127) {
    slider[currentGamepad][1] = 127;
  }

  // Slider
  gp[currentGamepad].x = slider[currentGamepad][0];
  gp[currentGamepad].y = slider[currentGamepad][1];
}

void handleButtons() {
  // Read the matrix
  for (int i = 0; i < 3; i++) {
    // Set the current row pin to LOW
    digitalWrite(rows[i], LOW);

    // Read all column pins
    for (int j = 0; j < 8; j++) {
      bool currentState = digitalRead(cols[j]);
      int buttonNumber = i * 8 + j;  // Calculate the button number

      // Update the button state
      if (currentState) {
        buttonStates[i][j]--;
      } else {
        buttonStates[i][j]++;
      }

      // Trigger calculation with Debounce
      if (buttonStates[i][j] == DEBOUNCE) {
        gp[currentGamepad].buttons |= (1U << buttonNumber);
      }
      // TODO RESET ON PAGE SWITCH
      if (buttonStates[i][j] == -DEBOUNCE) {
        gp[currentGamepad].buttons &= ~(1U << buttonNumber);
      }

      // Set max values
      if (buttonStates[i][j] > DEBOUNCE) {
        buttonStates[i][j] = DEBOUNCE + 1;
      }
      if (buttonStates[i][j] < -DEBOUNCE) {
        buttonStates[i][j] = DEBOUNCE - 1;
      }
    }

    // Set the current row pin back to HIGH
    digitalWrite(rows[i], HIGH);
  }
}

int getAddress(int p, int i, int j) {
  // Page is encoded byte 6 upwards so we have 4 bytes for label position
  // For the string we need 4 bytes so we have 2 bytes for the chars
  int addr = 0;
  addr |= (p << 6);
  addr |= (i << 2);
  addr |= (j << 0);
  //Serial.printf("[ %d:%d:%d - %d ] \r\n", p, i, j, addr);
  return addr;
}

// Function to save the labels
void saveLabels() {
  int addr;

  for (int p = 0; p < PAGES; p++) {
    for (int i = 0; i < 13; i++) {
      for (int j = 0; j < 4; j++) {
        addr = getAddress(p, i, j);
        EEPROM.write(addr, labels[p][i][j]);
      }
    }
  }

  if (EEPROM.commit()) {
    Serial.println("EEPROM successfully committed");
  } else {
    Serial.println("EEPROM commit failed");
  }
}

// Function to load the labels
void loadLabels() {
  int addr;

  for (int p = 0; p < PAGES; p++) {
    for (int i = 0; i < 13; i++) {
      for (int j = 0; j < 4; j++) {
        addr = getAddress(p, i, j);
        labels[p][i][j] = EEPROM.read(addr);
      }
    }
  }
}