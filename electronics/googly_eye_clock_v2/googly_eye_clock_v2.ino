/*
 * Googly Eyes Clock
 * In the spirit of the antique Fritz Oswald Rolling Eye clocks (1927-1950), this is a modern take on it with some
 * twists, pun intended, using the Arduino platform. For a bit more about his invention:
 * https://watchismo.blogspot.com/2007/09/rolling-eye-clocks-of-oswald-1927-1950.html
 * 
 * Electronic components:
 *   stepper for hour hand
 *   stepper for minute hand
 *   momentary switches (buttons)
 */
#include "Timer.h"
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"
#include <Bounce2.h>

void stepperHourMoveOne(int);
void stepperMinMoveOne(int);

// The steppers I'm using I pulled from something I don't remember. Likely a printer. They can
// step with 24 or 48 steps per revolution. I'm using 48 steps for the movement.
// hour hand: 48 steps per 12 hours; 48 steps per 43200000 ms; 1 step per 900000 ms
// min hand: 48 steps per 1 hr; 48 steps per per 3600000 ms; 1 step per 75000 ms

Adafruit_MotorShield motorShield = Adafruit_MotorShield();
Adafruit_StepperMotor *stepperHour = motorShield.getStepper(200, 1);
Adafruit_StepperMotor *stepperMin = motorShield.getStepper(200, 2);

Timer timer;
int timerIdHour = 0, timerIdMin = 0;
int oldHourBtnState = HIGH, oldMinBtnState = HIGH, oldGooglyBtnState = HIGH;
int manualStepDelay = 100; // 50ms
int stepStartHour = 0, stepStartMin = 0;
boolean runningHourManually = false, runningMinManually = false, runningGooglyManually = false;
int googlyMode = 0;

// 12 hrs (43200000 ms) / 48 steps/rev = 900000 ms; change to 1000 during development
const int MS_PER_STEP_HOUR = 900000;

// 60 mins (3600000 ms) / 48 steps/rev = 75000 ms;  change to 1000 during development
const int MS_PER_STEP_MIN = 75000;

const int PIN_BTN_HOUR = 2;
const int PIN_BTN_MIN = 3;
const int PIN_BTN_GOOGLY = 4;
Bounce btnHour = Bounce();
Bounce btnMin = Bounce();
Bounce btnGoogly = Bounce();

void setup() {
	Serial.begin(9600);
	Serial.println("setup");

	motorShield.begin();

	pinMode(PIN_BTN_HOUR, INPUT_PULLUP);
	pinMode(PIN_BTN_MIN, INPUT_PULLUP);
	pinMode(PIN_BTN_GOOGLY, INPUT_PULLUP);
	btnHour.attach(PIN_BTN_HOUR);
	btnMin.attach(PIN_BTN_MIN);
	btnGoogly.attach(PIN_BTN_GOOGLY);
	btnHour.interval(5);
	btnMin.interval(5);
	btnGoogly.interval(5);

	setupTimers();
}

void loop() {

	checkGooglyButtonState();
	if (runningGooglyManually) {
		return;
	}

	checkHourButtonState();
	checkMinButtonState();

	if (runningHourManually && runningMinManually) {
		return;
	}

	if (timerIdHour != 0 || timerIdMin != 0) {
		timer.update();
	}
}

void checkGooglyButtonState() {
	btnGoogly.update();
	int btnState = btnGoogly.read();

	if (oldGooglyBtnState == HIGH && btnState == LOW) {
		Serial.println("HL");
		stopTimer(&timerIdHour);
		stopTimer(&timerIdMin);
		googlyMode = random(1, 4);
		runningGooglyManually = true;
	} else if (oldGooglyBtnState == LOW && btnState == LOW) {
		Serial.println("LL");
		if (googlyMode == 1) {
			stepperHourMoveOne(BACKWARD);
			stepperMinMoveOne(BACKWARD);
		} else if (googlyMode == 2) {
			stepperHourMoveOne(BACKWARD);
			stepperMinMoveOne(FORWARD);
		} else if (googlyMode == 3) {
			stepperHourMoveOne(FORWARD);
			stepperMinMoveOne(FORWARD);
		}
	} else if (oldGooglyBtnState == LOW && btnState == HIGH) {
		Serial.println("LH");
		runningGooglyManually = false;
		setupTimers();
	}

    oldGooglyBtnState = btnState;
}

void checkHourButtonState() {
	btnHour.update();
	int btnState = btnHour.read();
	checkButtonState(&oldHourBtnState, btnState, &stepperHourMoveOne, &stepStartHour, &runningHourManually, &timerIdHour, &setupHourTimer);
	oldHourBtnState = btnState;
}

void checkMinButtonState() {
	btnMin.update();
	int btnState = btnMin.read();
	checkButtonState(&oldMinBtnState, btnState, &stepperMinMoveOne, &stepStartMin, &runningMinManually, &timerIdMin, &setupMinTimer);
	oldMinBtnState = btnState;
}

/*
 * Based on the state of the button, do something.
 */
void checkButtonState(int* pOldBtnState, int btnState, void (*funcMoveStepper)(int), int* pStepStart, boolean* pRunningManually, int* pTimerId, void (*funcSetupTimer)(void)) {

	// Button was not pushed and button is now pushed. We stop the timers and begin
	// the rotation of the stepper.
	if (*pOldBtnState == HIGH && btnState == LOW) {
		Serial.println("HL");
		stopTimer(pTimerId);
		stepperManualStart(pStepStart, pRunningManually);
		return;
	}

	// Button was pushed and button is now still pushed. We continue the rotation of the
	// stepper.
	if (*pOldBtnState == LOW && btnState == LOW) {
		Serial.println("LL");
		int now = millis();
		if ((now - *pStepStart) > manualStepDelay) {
			funcMoveStepper(FORWARD);
			*pStepStart = now;
		}
		return;
	}

	// Button was pushed and button is now not pushed. We stop the rotation of the stepper
	// and restart the timers to begin tell time.
	if (*pOldBtnState == LOW && btnState == HIGH) {
		Serial.println("LH");
		stepperManualStop(pStepStart, pRunningManually);
		funcSetupTimer();
		return;
	}

	// Button was not pushed and button is still not pushed. We do nothing.
}

void stopTimer(int* pTimerId) {
	timer.stop(*pTimerId);
	*pTimerId = 0;
}

void setupTimers() {
	setupHourTimer();
	setupMinTimer();
}

void setupHourTimer() {
	timerIdHour = timer.every(MS_PER_STEP_HOUR, stepperHourMoveOne);
	Serial.print(timerIdHour);
}

void setupMinTimer() {
	timerIdMin = timer.every(MS_PER_STEP_MIN, stepperMinMoveOne);
	Serial.print(timerIdMin);
}

void stepperHourMoveOne(int dir) {
	//Serial.println("hour 1 step");
	// For my stepper, INTERLEAVE is 1/48th of a step
	stepperHour->step(1, dir, INTERLEAVE);
}

void stepperMinMoveOne(int dir) {
	//Serial.println("min 1 step");
	// For my stepper, INTERLEAVE is 1/48th of a step
	stepperMin->step(1, dir, INTERLEAVE);
}

void stepperManualStart(int* pStepStart, boolean* pRunningManually) {
	Serial.println("stepper manual start");
	*pStepStart = millis();
	*pRunningManually = true;
}

void stepperManualStop(int* pStepStart, boolean* pRunningManually) {
	Serial.println("stepper manual stop");
	*pRunningManually = false;
	*pStepStart = 0;
}
