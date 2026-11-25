#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <HX711.h>
#include <ESP32Servo.h>

// =====================================================
// OLED
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22

Adafruit_SH1106G display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);


// =====================================================
// JOYSTICK
// SW IS NOT USED
// =====================================================

#define JOY_X 34
#define JOY_Y 35

#define JOY_LOW 1000
#define JOY_HIGH 3500

#define JOY_NEUTRAL_LOW 1500
#define JOY_NEUTRAL_HIGH 3300


// =====================================================
// START / STOP BUTTONS
// =====================================================

#define START_BUTTON 25
#define STOP_BUTTON 26


// =====================================================
// HX711
// =====================================================

#define HX711_DT 32
#define HX711_SCK 33

HX711 scale;

// Calibration from your previous test
float calibrationFactor = 1002.0;

// Small readings become zero
const float ZERO_THRESHOLD = 0.5;

// Minimum weight before detecting an item
const float ITEM_DETECT_WEIGHT = 2.0;

// Item considered removed below this
const float ITEM_REMOVED_WEIGHT = 1.0;


// =====================================================
// SERVO
// =====================================================

#define SERVO_PIN 18

Servo sortingServo;

// Center
const int SERVO_HOME = 90;

// ACCEPT -> clockwise
const int SERVO_ACCEPT = 180;

// UNDER / OVER -> counterclockwise
const int SERVO_REJECT = 0;


// =====================================================
// SETTINGS
// =====================================================

int targetWeight = 15;
int tolerance = 2;

const int MIN_TARGET = 1;
const int MAX_TARGET = 500;

const int MIN_TOLERANCE = 1;
const int MAX_TOLERANCE = 50;

int oldTargetWeight = 15;
int oldTolerance = 2;


// =====================================================
// SYSTEM MODE
// =====================================================

enum SystemState {
  SETTINGS_MODE,
  RUN_MODE
};

SystemState systemState = SETTINGS_MODE;


// =====================================================
// SETTINGS SCREEN STATE
// =====================================================

enum ScreenState {
  MAIN_MENU,
  EDIT_TARGET,
  EDIT_TOLERANCE
};

ScreenState currentScreen = MAIN_MENU;

int selectedMenu = 0;


// =====================================================
// WEIGHING STATE
// =====================================================

enum WeighState {
  WAITING_FOR_ITEM,
  WEIGHING_ITEM,
  SHOWING_RESULT,
  SERVO_MOVING,
  WAITING_FOR_REMOVAL
};

WeighState weighState = WAITING_FOR_ITEM;


// =====================================================
// RESULT STATE
// =====================================================

enum WeightResult {
  RESULT_NONE,
  RESULT_UNDER,
  RESULT_ACCEPT,
  RESULT_OVER
};

WeightResult weightResult = RESULT_NONE;


// =====================================================
// WEIGHT VARIABLES
// =====================================================

float currentWeight = 0.0;
float previousWeight = 0.0;
float lockedWeight = 0.0;

float filteredWeight = 0.0;
bool filterInitialized = false;

// Smoothing
const float FILTER_ALPHA = 0.45;

// Stable reading logic
int stableCount = 0;

const int REQUIRED_STABLE_READINGS = 5;

const float STABLE_THRESHOLD = 0.30;


// =====================================================
// TIMING
// =====================================================

unsigned long lastJoystickAction = 0;
unsigned long lastWeightRead = 0;
unsigned long lastAnimationTime = 0;

unsigned long weighStartTime = 0;
unsigned long resultStartTime = 0;
unsigned long servoStartTime = 0;

const unsigned long JOYSTICK_DELAY = 250;

const unsigned long WEIGHT_INTERVAL = 100;

const unsigned long ANIMATION_DELAY = 100;

// Minimum time item must be weighed
const unsigned long MIN_WEIGH_TIME = 2500;

// Result shown before servo moves
const unsigned long RESULT_DISPLAY_TIME = 2500;

// Servo stays at endpoint
const unsigned long SERVO_HOLD_TIME = 800;


// =====================================================
// CONVEYOR ANIMATION
// =====================================================

int packageX = -12;


// =====================================================
// START BUTTON
// =====================================================

bool startPressed() {

  static bool previousState = HIGH;

  bool currentState = digitalRead(START_BUTTON);

  bool pressed = false;

  if (
    previousState == HIGH &&
    currentState == LOW
  ) {

    pressed = true;
  }

  previousState = currentState;

  return pressed;
}


// =====================================================
// STOP BUTTON
// =====================================================

bool stopPressed() {

  static bool previousState = HIGH;

  bool currentState = digitalRead(STOP_BUTTON);

  bool pressed = false;

  if (
    previousState == HIGH &&
    currentState == LOW
  ) {

    pressed = true;
  }

  previousState = currentState;

  return pressed;
}


// =====================================================
// JOYSTICK HELPERS
// =====================================================

bool xNeutral(int x) {

  return (
    x >= JOY_NEUTRAL_LOW &&
    x <= JOY_NEUTRAL_HIGH
  );
}


bool yNeutral(int y) {

  return (
    y >= JOY_NEUTRAL_LOW &&
    y <= JOY_NEUTRAL_HIGH
  );
}


bool joystickReady() {

  return (
    millis() - lastJoystickAction >=
    JOYSTICK_DELAY
  );
}


void joystickActionTaken() {

  lastJoystickAction = millis();
}


// =====================================================
// MAIN MENU
// =====================================================

void drawMainMenu() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(0, 0);
  display.println("WEIGHT SETTINGS");

  display.drawLine(
    0, 11,
    127, 11,
    SH110X_WHITE
  );


  // TARGET
  if (selectedMenu == 0) {

    display.fillRect(
      0, 17,
      128, 18,
      SH110X_WHITE
    );

    display.setTextColor(SH110X_BLACK);

    display.setCursor(4, 22);

    display.print("Target: ");
    display.print(targetWeight);
    display.print("g");

    display.setCursor(116, 22);
    display.print(">");
  }

  else {

    display.setTextColor(SH110X_WHITE);

    display.setCursor(4, 22);

    display.print("Target: ");
    display.print(targetWeight);
    display.print("g");

    display.setCursor(116, 22);
    display.print(">");
  }


  // TOLERANCE
  if (selectedMenu == 1) {

    display.fillRect(
      0, 38,
      128, 18,
      SH110X_WHITE
    );

    display.setTextColor(SH110X_BLACK);

    display.setCursor(4, 43);

    display.print("Tolerance: ");
    display.print(tolerance);
    display.print("g");

    display.setCursor(116, 43);
    display.print(">");
  }

  else {

    display.setTextColor(SH110X_WHITE);

    display.setCursor(4, 43);

    display.print("Tolerance: ");
    display.print(tolerance);
    display.print("g");

    display.setCursor(116, 43);
    display.print(">");
  }


  display.display();
}


// =====================================================
// TARGET EDITOR
// =====================================================

void drawTargetEditor() {

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("TARGET WEIGHT");

  display.drawLine(
    0, 11,
    127, 11,
    SH110X_WHITE
  );


  display.setTextSize(2);

  display.setCursor(5, 24);
  display.print("<");

  display.setCursor(43, 24);
  display.print(targetWeight);
  display.print("g");

  display.setCursor(115, 24);
  display.print(">");


  display.setTextSize(1);

  display.setCursor(8, 53);
  display.print("^ SAVE");

  display.setCursor(76, 53);
  display.print("v EXIT");


  display.display();
}


// =====================================================
// TOLERANCE EDITOR
// =====================================================

void drawToleranceEditor() {

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("TOLERANCE");

  display.drawLine(
    0, 11,
    127, 11,
    SH110X_WHITE
  );


  display.setTextSize(2);

  display.setCursor(5, 24);
  display.print("<");

  display.setCursor(43, 24);
  display.print(tolerance);
  display.print("g");

  display.setCursor(115, 24);
  display.print(">");


  display.setTextSize(1);

  display.setCursor(8, 53);
  display.print("^ SAVE");

  display.setCursor(76, 53);
  display.print("v EXIT");


  display.display();
}


// =====================================================
// RUNNING / WAITING SCREEN
// =====================================================

void drawWaitingScreen() {

  int minimumWeight =
    targetWeight - tolerance;

  int maximumWeight =
    targetWeight + tolerance;

  if (minimumWeight < 0) {
    minimumWeight = 0;
  }


  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("SYSTEM RUNNING");

  display.drawLine(
    0, 11,
    127, 11,
    SH110X_WHITE
  );


  display.setCursor(27, 15);
  display.print("TARGET RANGE");


  display.setTextSize(2);

  display.setCursor(10, 26);

  display.print(">");
  display.print(minimumWeight);
  display.print("-");
  display.print(maximumWeight);
  display.print("g");
  display.print("<");


  // Conveyor
  display.drawLine(
    0, 53,
    127, 53,
    SH110X_WHITE
  );


  // Rollers
  for (
    int x = 4;
    x <= 124;
    x += 12
  ) {

    display.drawCircle(
      x,
      59,
      2,
      SH110X_WHITE
    );
  }


  // Package
  display.fillRect(
    packageX,
    43,
    12,
    9,
    SH110X_WHITE
  );


  display.drawLine(
    packageX + 6,
    43,
    packageX + 6,
    52,
    SH110X_BLACK
  );


  display.display();
}


// =====================================================
// CONVEYOR ANIMATION
// =====================================================

void updateWaitingAnimation() {

  if (
    millis() - lastAnimationTime >=
    ANIMATION_DELAY
  ) {

    lastAnimationTime = millis();

    packageX += 3;

    if (packageX > 127) {

      packageX = -12;
    }

    drawWaitingScreen();
  }
}


// =====================================================
// WEIGHING SCREEN
// =====================================================

void drawWeighingScreen(float weight) {

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);


  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("WEIGHING ITEM");

  display.drawLine(
    0, 11,
    127, 11,
    SH110X_WHITE
  );


  display.setTextSize(3);

  display.setCursor(8, 22);

  display.print(weight, 1);


  display.setTextSize(2);

  display.print("g");


  display.setTextSize(1);

  display.setCursor(22, 55);

  display.print("STABILIZING...");


  display.display();
}


// =====================================================
// RESULT SCREEN
// =====================================================

void drawResultScreen() {

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);


  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("WEIGHT RESULT");

  display.drawLine(
    0, 11,
    127, 11,
    SH110X_WHITE
  );


  display.setTextSize(2);

  display.setCursor(28, 20);

  display.print(lockedWeight, 1);
  display.print("g");


  display.setTextSize(2);


  if (
    weightResult ==
    RESULT_UNDER
  ) {

    display.setCursor(33, 44);
    display.print("UNDER");
  }


  else if (
    weightResult ==
    RESULT_ACCEPT
  ) {

    display.setCursor(26, 44);
    display.print("ACCEPT");
  }


  else if (
    weightResult ==
    RESULT_OVER
  ) {

    display.setCursor(38, 44);
    display.print("OVER");
  }


  display.display();
}


// =====================================================
// SORTING SCREEN
// =====================================================

void drawSortingScreen() {

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);


  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("ITEM SORTING");

  display.drawLine(
    0, 11,
    127, 11,
    SH110X_WHITE
  );


  display.setTextSize(2);

  display.setCursor(28, 22);

  display.print(lockedWeight, 1);
  display.print("g");


  display.setTextSize(1);

  display.setCursor(35, 52);

  display.print("SORTING...");


  display.display();
}


// =====================================================
// WAIT FOR SCALE CLEAR
// =====================================================

void drawRemovalScreen() {

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);


  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("ITEM COMPLETE");

  display.drawLine(
    0, 11,
    127, 11,
    SH110X_WHITE
  );


  display.setCursor(18, 26);
  display.println("WAITING FOR");

  display.setCursor(15, 40);
  display.println("SCALE TO CLEAR");


  display.display();
}


// =====================================================
// READ WEIGHT
// =====================================================

float readWeight() {

  if (!scale.is_ready()) {

    return currentWeight;
  }


  float rawWeight =
    scale.get_units(1);


  if (rawWeight < 0) {

    rawWeight = 0;
  }


  if (
    abs(rawWeight) <
    ZERO_THRESHOLD
  ) {

    rawWeight = 0;
  }


  if (!filterInitialized) {

    filteredWeight =
      rawWeight;

    filterInitialized =
      true;
  }

  else {

    filteredWeight =
      (
        FILTER_ALPHA *
        rawWeight
      )
      +
      (
        (1.0 - FILTER_ALPHA) *
        filteredWeight
      );
  }


  if (
    filteredWeight <
    ZERO_THRESHOLD
  ) {

    filteredWeight = 0;
  }


  return filteredWeight;
}


// =====================================================
// CLASSIFY WEIGHT
// =====================================================

void classifyWeight() {

  float minimumWeight =
    targetWeight - tolerance;

  float maximumWeight =
    targetWeight + tolerance;


  if (
    lockedWeight <
    minimumWeight
  ) {

    weightResult =
      RESULT_UNDER;
  }

  else if (
    lockedWeight >
    maximumWeight
  ) {

    weightResult =
      RESULT_OVER;
  }

  else {

    weightResult =
      RESULT_ACCEPT;
  }
}


// =====================================================
// WEIGHT PROCESS
// =====================================================

void processWeight() {

  // Animation while waiting
  if (
    weighState ==
    WAITING_FOR_ITEM
  ) {

    updateWaitingAnimation();
  }


  // New HX711 reading interval
  if (
    millis() - lastWeightRead <
    WEIGHT_INTERVAL
  ) {

    return;
  }


  lastWeightRead =
    millis();


  currentWeight =
    readWeight();


  Serial.print("Weight: ");

  Serial.print(
    currentWeight,
    2
  );

  Serial.println(" g");


  // =================================================
  // WAITING FOR ITEM
  // =================================================

  if (
    weighState ==
    WAITING_FOR_ITEM
  ) {

    if (
      currentWeight >=
      ITEM_DETECT_WEIGHT
    ) {

      previousWeight =
        currentWeight;

      stableCount = 0;

      // Start minimum weighing timer
      weighStartTime =
        millis();

      weighState =
        WEIGHING_ITEM;


      drawWeighingScreen(
        currentWeight
      );
    }

    return;
  }


  // =================================================
  // WEIGHING ITEM
  // =================================================

  if (
    weighState ==
    WEIGHING_ITEM
  ) {

    drawWeighingScreen(
      currentWeight
    );


    float difference =
      abs(
        currentWeight -
        previousWeight
      );


    if (
      difference <=
      STABLE_THRESHOLD
    ) {

      stableCount++;
    }

    else {

      stableCount = 0;
    }


    previousWeight =
      currentWeight;


    // =================================================
    // MUST BE STABLE
    // AND
    // MUST HAVE BEEN WEIGHING FOR AT LEAST 2.5 SEC
    // =================================================

    if (
      stableCount >=
        REQUIRED_STABLE_READINGS
      &&
      millis() -
        weighStartTime >=
        MIN_WEIGH_TIME
    ) {

      lockedWeight =
        currentWeight;


      classifyWeight();


      Serial.print(
        "LOCKED WEIGHT: "
      );

      Serial.print(
        lockedWeight,
        2
      );

      Serial.println(" g");


      drawResultScreen();


      resultStartTime =
        millis();


      weighState =
        SHOWING_RESULT;
    }

    return;
  }


  // =================================================
  // SHOW RESULT
  // =================================================

  if (
    weighState ==
    SHOWING_RESULT
  ) {

    if (
      millis() -
      resultStartTime >=
      RESULT_DISPLAY_TIME
    ) {

      // ACCEPT
      if (
        weightResult ==
        RESULT_ACCEPT
      ) {

        sortingServo.write(
          SERVO_ACCEPT
        );
      }

      // UNDER / OVER
      else {

        sortingServo.write(
          SERVO_REJECT
        );
      }


      servoStartTime =
        millis();


      drawSortingScreen();


      weighState =
        SERVO_MOVING;
    }

    return;
  }


  // =================================================
  // SERVO MOVING
  // =================================================

  if (
    weighState ==
    SERVO_MOVING
  ) {

    if (
      millis() -
      servoStartTime >=
      SERVO_HOLD_TIME
    ) {

      sortingServo.write(
        SERVO_HOME
      );


      drawRemovalScreen();


      weighState =
        WAITING_FOR_REMOVAL;
    }

    return;
  }


  // =================================================
  // WAIT FOR ITEM REMOVAL
  // =================================================

  if (
    weighState ==
    WAITING_FOR_REMOVAL
  ) {

    if (
      currentWeight <=
      ITEM_REMOVED_WEIGHT
    ) {

      stableCount = 0;

      lockedWeight = 0;

      previousWeight = 0;

      currentWeight = 0;

      filteredWeight = 0;

      filterInitialized = false;

      weightResult =
        RESULT_NONE;

      packageX = -12;


      weighState =
        WAITING_FOR_ITEM;


      drawWaitingScreen();
    }

    return;
  }
}


// =====================================================
// MAIN MENU CONTROL
// =====================================================

void handleMainMenu() {

  int x =
    analogRead(JOY_X);

  delay(3);

  int y =
    analogRead(JOY_Y);


  if (!joystickReady()) {
    return;
  }


  // UP
  if (y < JOY_LOW) {

    selectedMenu--;

    if (
      selectedMenu < 0
    ) {

      selectedMenu = 1;
    }

    joystickActionTaken();

    drawMainMenu();

    return;
  }


  // DOWN
  if (y > JOY_HIGH) {

    selectedMenu++;

    if (
      selectedMenu > 1
    ) {

      selectedMenu = 0;
    }

    joystickActionTaken();

    drawMainMenu();

    return;
  }


  // RIGHT = SELECT
  if (
    x > JOY_HIGH &&
    yNeutral(y)
  ) {

    joystickActionTaken();


    if (
      selectedMenu == 0
    ) {

      oldTargetWeight =
        targetWeight;

      currentScreen =
        EDIT_TARGET;

      drawTargetEditor();
    }

    else {

      oldTolerance =
        tolerance;

      currentScreen =
        EDIT_TOLERANCE;

      drawToleranceEditor();
    }


    return;
  }
}


// =====================================================
// TARGET EDITOR CONTROL
// =====================================================

void handleTargetEditor() {

  int x =
    analogRead(JOY_X);

  delay(3);

  int y =
    analogRead(JOY_Y);


  if (!joystickReady()) {
    return;
  }


  // UP = SAVE
  if (
    y < JOY_LOW &&
    xNeutral(x)
  ) {

    currentScreen =
      MAIN_MENU;

    joystickActionTaken();

    drawMainMenu();

    return;
  }


  // DOWN = CANCEL
  if (
    y > JOY_HIGH &&
    xNeutral(x)
  ) {

    targetWeight =
      oldTargetWeight;

    currentScreen =
      MAIN_MENU;

    joystickActionTaken();

    drawMainMenu();

    return;
  }


  // LEFT
  if (
    x < JOY_LOW &&
    yNeutral(y)
  ) {

    targetWeight--;

    if (
      targetWeight <
      MIN_TARGET
    ) {

      targetWeight =
        MIN_TARGET;
    }

    joystickActionTaken();

    drawTargetEditor();

    return;
  }


  // RIGHT
  if (
    x > JOY_HIGH &&
    yNeutral(y)
  ) {

    targetWeight++;

    if (
      targetWeight >
      MAX_TARGET
    ) {

      targetWeight =
        MAX_TARGET;
    }

    joystickActionTaken();

    drawTargetEditor();

    return;
  }
}


// =====================================================
// TOLERANCE EDITOR CONTROL
// =====================================================

void handleToleranceEditor() {

  int x =
    analogRead(JOY_X);

  delay(3);

  int y =
    analogRead(JOY_Y);


  if (!joystickReady()) {
    return;
  }


  // UP = SAVE
  if (
    y < JOY_LOW &&
    xNeutral(x)
  ) {

    currentScreen =
      MAIN_MENU;

    joystickActionTaken();

    drawMainMenu();

    return;
  }


  // DOWN = CANCEL
  if (
    y > JOY_HIGH &&
    xNeutral(x)
  ) {

    tolerance =
      oldTolerance;

    currentScreen =
      MAIN_MENU;

    joystickActionTaken();

    drawMainMenu();

    return;
  }


  // LEFT
  if (
    x < JOY_LOW &&
    yNeutral(y)
  ) {

    tolerance--;

    if (
      tolerance <
      MIN_TOLERANCE
    ) {

      tolerance =
        MIN_TOLERANCE;
    }

    joystickActionTaken();

    drawToleranceEditor();

    return;
  }


  // RIGHT
  if (
    x > JOY_HIGH &&
    yNeutral(y)
  ) {

    tolerance++;

    if (
      tolerance >
      MAX_TOLERANCE
    ) {

      tolerance =
        MAX_TOLERANCE;
    }

    joystickActionTaken();

    drawToleranceEditor();

    return;
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);


  analogReadResolution(12);


  // Buttons
  pinMode(
    START_BUTTON,
    INPUT_PULLUP
  );

  pinMode(
    STOP_BUTTON,
    INPUT_PULLUP
  );


  // OLED
  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  if (
    !display.begin(
      0x3C,
      true
    )
  ) {

    Serial.println(
      "OLED ERROR!"
    );

    while (true) {
      delay(1000);
    }
  }


  display.clearDisplay();

  display.setTextColor(
    SH110X_WHITE
  );

  display.setTextSize(1);

  display.setCursor(18, 18);

  display.println(
    "WEIGHT SORTER"
  );

  display.setCursor(25, 35);

  display.println(
    "Starting..."
  );

  display.display();


  // =================================================
  // HX711
  // =================================================

  scale.begin(
    HX711_DT,
    HX711_SCK
  );


  if (
    !scale.wait_ready_timeout(
      3000
    )
  ) {

    display.clearDisplay();

    display.setTextSize(1);

    display.setCursor(22, 25);

    display.println(
      "HX711 ERROR!"
    );

    display.display();


    while (true) {
      delay(1000);
    }
  }


  scale.set_scale(
    calibrationFactor
  );


  // =================================================
  // SERVO
  // =================================================

  sortingServo.setPeriodHertz(50);

  sortingServo.attach(
    SERVO_PIN,
    500,
    2400
  );


  sortingServo.write(
    SERVO_HOME
  );


  // =================================================
  // TARE
  // =================================================

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(7, 18);

  display.println(
    "KEEP SCALE EMPTY"
  );

  display.setCursor(40, 36);

  display.println(
    "TARING..."
  );

  display.display();


  delay(2000);


  scale.tare(20);


  Serial.println(
    "Tare complete."
  );


  display.clearDisplay();

  display.setCursor(25, 27);

  display.println(
    "TARE COMPLETE"
  );

  display.display();


  delay(1000);


  drawMainMenu();
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // START
  if (startPressed()) {

    systemState =
      RUN_MODE;

    weighState =
      WAITING_FOR_ITEM;

    stableCount = 0;

    lockedWeight = 0;

    previousWeight = 0;

    currentWeight = 0;

    filteredWeight = 0;

    filterInitialized = false;

    weightResult =
      RESULT_NONE;

    packageX = -12;


    sortingServo.write(
      SERVO_HOME
    );


    drawWaitingScreen();
  }


  // STOP
  if (stopPressed()) {

    systemState =
      SETTINGS_MODE;

    currentScreen =
      MAIN_MENU;


    sortingServo.write(
      SERVO_HOME
    );


    drawMainMenu();
  }


  // RUN MODE
  if (
    systemState ==
    RUN_MODE
  ) {

    processWeight();

    delay(5);

    return;
  }


  // SETTINGS MODE
  switch (currentScreen) {

    case MAIN_MENU:

      handleMainMenu();

      break;


    case EDIT_TARGET:

      handleTargetEditor();

      break;


    case EDIT_TOLERANCE:

      handleToleranceEditor();

      break;
  }


  delay(10);
}
