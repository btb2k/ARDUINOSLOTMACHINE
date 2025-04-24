// Enhanced Slot Machine Sketch with Reels and Bonus
// Libraries: Keypad, LiquidCrystal_I2C, AccelStepper

#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>

// ----- Hardware Setup -----
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte rowPins[ROWS] = {31,33,35,37};
byte colPins[COLS] = {39,41,43,45};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(0x27,20,4);

// Steppers (DRV8825)
AccelStepper reel1(AccelStepper::DRIVER, 40, 42);
AccelStepper reel2(AccelStepper::DRIVER, 44, 46);
AccelStepper reel3(AccelStepper::DRIVER, 48, 49);

// LEDs
const int reelLed[3]  = {22, 33, 24};
const int payLeds[15] = {2,3,4,6,7,8,9,11,12,13,14,15,16,17,18};

// Constants
const long STEPS_PER_REV = 200*16;
const int NUM_STOPS = 11;
enum Symbol {CHERRY,LEMON,ORANGE,PLUM,GRAPE,WATERMELON,BELL,BAR,BONUS_SYM,JACKPOT_SYM,SEVEN};

int bets[3]  = {5,10,15};
int mults[3] = {1,2,3};
uint32_t balance = 0;
uint32_t jackpot = 0;
int freeSpins = 0;

// Prototypes
void setup();
void loop();
void showMenu();
void addFunds();
void playReels();
void cashOut();
void bonusGame(int bet);
String readDigits(int len);
void ledChase();

// ----- Entry Points -----
void setup() {
  Wire.begin();
  lcd.init(); lcd.backlight();
  randomSeed(analogRead(A7));

  // Init steppers
  reel1.setMaxSpeed(2000); reel1.setAcceleration(500);
  reel2.setMaxSpeed(2000); reel2.setAcceleration(500);
  reel3.setMaxSpeed(2000); reel3.setAcceleration(500);

  // Init LEDs
  for (int i = 0; i < 3; i++) {
    pinMode(reelLed[i], OUTPUT);
    digitalWrite(reelLed[i], LOW);
  }
  for (int i = 0; i < 15; i++) {
    pinMode(payLeds[i], OUTPUT);
    digitalWrite(payLeds[i], LOW);
  }
}

void loop() {
  showMenu();
}

// ----- Main Menu -----
void showMenu() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("1:Add Funds");
  lcd.setCursor(0,1); lcd.print("2:Play Reels");
  lcd.setCursor(0,2); lcd.print("3:Cash Out");
  lcd.setCursor(0,3); lcd.print("D:Reset Bal");
  char k = keypad.waitForKey();

  if (k == '1') addFunds();
  else if (k == '2') playReels();
  else if (k == '3') cashOut();
  else if (k == 'D') {
    balance = 0;
    lcd.clear(); lcd.print("Balance reset");
    delay(1000);
  }
}

// ----- Add Funds -----
void addFunds() {
  lcd.clear(); lcd.print("Enter amount:");
  String s = readDigits(5);
  uint32_t amt = s.toInt();
  balance += amt;

  lcd.clear(); lcd.print("Added: "); lcd.print(amt);
  lcd.setCursor(0,1); lcd.print("Bal: "); lcd.print(balance);
  delay(1500);
}

// ----- Play Reels -----
void playReels() {
  if (balance == 0) {
    lcd.clear(); lcd.print("No funds! Add");
    delay(1500);
    return;
  }
  int betIdx = 0, winCount = 0;

  while (true) {
    // Display status
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Bal:"); lcd.print(balance);
    lcd.setCursor(0,1); lcd.print("Bet:"); lcd.print(bets[betIdx]);
    lcd.setCursor(0,2); lcd.print("A:Spin B:Bet");
    lcd.setCursor(0,3); lcd.print("D:Back");

    char c;
    do { c = keypad.waitForKey(); } while (c!='A' && c!='B' && c!='D');
    if (c == 'D') return;
    if (c == 'B') { betIdx = (betIdx + 1) % 3; continue; }

    int bet = bets[betIdx];
    if (freeSpins > 0) {
      freeSpins--;
      lcd.clear(); lcd.print("Free Spin");
      delay(1000);
    } else if (balance >= bet) {
      balance -= bet;
      jackpot += 10;
    } else {
      lcd.clear(); lcd.print("Insufficient");
      delay(1000);
      continue;
    }

    int stops[3];
    for (int r = 0; r < 3; r++) stops[r] = random(NUM_STOPS);
    long stepsPerStop = STEPS_PER_REV / NUM_STOPS;

    reel1.setCurrentPosition(0); reel2.setCurrentPosition(0); reel3.setCurrentPosition(0);
    reel1.setSpeed(800); reel2.setSpeed(800); reel3.setSpeed(800);
    unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
      reel1.runSpeed(); reel2.runSpeed(); reel3.runSpeed();
    }
    reel1.setSpeed(0); reel2.setSpeed(0); reel3.setSpeed(0);

    for (int r = 0; r < 3; r++) {
      AccelStepper &motor = (r==0?reel1:(r==1?reel2:reel3));
      motor.moveTo(stops[r]*stepsPerStop);
      while (motor.distanceToGo()!=0) motor.run();
      digitalWrite(reelLed[r], HIGH); delay(150); digitalWrite(reelLed[r], LOW);
    }

    int cnt[NUM_STOPS]={0};
    for (int r=0;r<3;r++) cnt[stops[r]]++;
    uint32_t win=0;
    if(cnt[JACKPOT_SYM]==3){win=jackpot;jackpot=0;}
    else if(cnt[BONUS_SYM]==3){bonusGame(bet);}
    else if(cnt[SEVEN]==3) win=50*mults[betIdx];
    else if(cnt[BAR]==3)   win=100*mults[betIdx];
    else if(cnt[BELL]==3)  win=30*mults[betIdx];
    else if(cnt[WATERMELON]==3) win=20*mults[betIdx];
    else if(cnt[GRAPE]==3) win=15*mults[betIdx];
    else if(cnt[PLUM]==3)  win=12*mults[betIdx];
    else if(cnt[LEMON]==3) win=10*mults[betIdx];
    else if(cnt[ORANGE]>=2) win=8*mults[betIdx];
    else if(cnt[CHERRY]>=1) win=5*mults[betIdx];

    balance += win;
    if(win>0) winCount++;

    if(win>0){
      unsigned long f1=millis();
      while(millis()-f1<2000){
        for(int i=0;i<3;i++) digitalWrite(reelLed[i],HIGH);
        for(int j=0;j<15;j++) digitalWrite(payLeds[j],HIGH);
        delay(200);
        for(int i=0;i<3;i++) digitalWrite(reelLed[i],LOW);
        for(int j=0;j<15;j++) digitalWrite(payLeds[j],LOW);
        delay(200);
      }
    }

    lcd.clear(); lcd.print("Win:"); lcd.print(win);
    lcd.setCursor(0,1); lcd.print("Wins:"); lcd.print(winCount);
    lcd.setCursor(0,2); lcd.print("Bal:"); lcd.print(balance);
    delay(1500);
  }
}

// ----- Cash Out -----
void cashOut() {
  lcd.clear(); lcd.print("Cashed out:"); lcd.setCursor(0,1); lcd.print(balance);
  balance=0; delay(1500);
}

// ----- Bonus Game -----
void bonusGame(int bet) {
  lcd.clear(); lcd.print("A:Pick B:Free C:Skip");
  char k;
  while(true) {
    k = keypad.waitForKey();
    if(k=='A'||k=='B'||k=='C') {
      if(k=='A') {
        lcd.clear(); lcd.print("Pick A B C"); char p;
        while(true) {
          p = keypad.waitForKey();
          if(p=='A'||p=='B'||p=='C') {
            int roll=random(100),pr;
            if(roll<30) pr=random(51,101);
            else if(roll<80) pr=bet*random(2,6);
            else pr=random(1,bet+1);
            balance+=pr;
            lcd.clear(); lcd.print("Win "); lcd.print(pr);
            delay(1000);
            break;
          }
        }
      } else if(k=='B') {
        freeSpins=5;
        lcd.clear(); lcd.print("Free x5"); delay(1000);
      } else {
        lcd.clear(); lcd.print("No Bonus"); delay(500);
      }
      break;
    }
  }
}

// ----- Utilities -----
String readDigits(int len) {
  String s="";
  while(s.length()<len) {
    char k=keypad.waitForKey();
    if(isdigit(k)) {
      s+=k;
      lcd.setCursor(s.length()-1,1);
      lcd.print(k);
    }
  }
  delay(300);
  return s;
}

void ledChase() {
  for(int i=0;i<3;i++) {
    digitalWrite(reelLed[i],HIGH);
    delay(100);
    digitalWrite(reelLed[i],LOW);
  }
}
