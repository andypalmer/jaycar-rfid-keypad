//RFID Tag Unlocker
//RFID reader + screen + relay (not many IO, but can use D1,D0 and A5, should be OK if outputs. TX/RX leds are driven by 16u2, so should be ignored)
//relay on A5 so it doens't chatter. D0 for RFID RST, as it idles high anyway
//master user is first user and only has settings access (can't unlock)
//save settings to eeprom 8bytes card, 8 bytes pin, 14 bytes username, 2 byte permissions (allow access on card, allow access on pin) gives 32 users on Uno

#include "XC4630d.c"
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <string.h>

#define RST_PIN 0
#define SS_PIN 10
#define USERCOUNT 32
#define RELAYTIME 1000

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance, could be static in getcard if we didn't have to do the init in setup... wrap it somehow?

struct UserData {
  byte card_id[8];
  byte pin[8];
  char name[14];
  byte card_allowed;
  byte pin_allowed;
};

typedef struct Point {
  int x;
  int y;
} Point;
#define Point(x,y) ((Point){(x),(y)})

typedef struct Size {
  int width;
  int height;
} Size;
#define Size(w,h) ((Size){(w),(h)})

// Used by dobutton and checktouch
int bx[] = {25, 95, 165, 25, 95, 165, 25, 95, 165, 25, 95, 165};
int by[] = {10, 10, 10, 70, 70, 70, 130, 130, 130, 190, 190, 190};
char bb[] = "123456789#0*";

// Used by drawkeyboard and checkkeyboard
char kb[] = "1234567890QWERTYUIOPASDFGHJKL'ZXCVBNM .<"; //soft keyboard

void setup() {
  SPI.begin();              //start SPI
  mfrc522.PCD_Init();       //start RC522 module
  pinMode(0, INPUT);        //use serial pullup to hold high
  XC4630_init();
  XC4630_rotate(1);
  UserData admin = get_user(0);
  if (!is_set(admin.card_id) && !is_set(admin.pin)) {
    dosetup(admin);  //first time setup if no master user set
  }
  draw_user_screen();
  Serial.begin(9600);
}

void clear_screen() {
  XC4630_clear(BLACK);
}

void draw_user_screen() {
  clear_screen();
  draw_pinpad();
  display_ready_message();
}

typedef char (*keyboard_function)(char*);

typedef struct {
  char key;
  keyboard_function function;
} specialkey;

void loop() {
  static char pin[9] = "";
  byte card_id[8];
  char a;
  int s;

  a = checktouch();
  
  static const specialkey keys[] = {{'#',erase_last_from}, {'*', dopin}};
  for(int i=0; i < 2; i++) {
    if (keys[i].key == a) {
      a = keys[i].function(pin);
    }
  }

  s = strlen(pin);
  if (a && s<8) {
    pin[s] = a;
    pin[s + 1] = 0;
  }
 
  s = strlen(pin);
  for (int i = 0; i < 8; i++) {
    XC4630_char(120 + i * 12, 260, ((i < s) ? '*' : ' '), GREY, BLACK);
  }
  
  if (checkcard(card_id)) {
    XC4630_chara(108, 280, "CARD", BLACK, WHITE);
    docard(card_id);                                   //process card
    delay(100);
  } else {
    XC4630_chara(108, 280, "CARD", GREY, BLACK);
  }
}

char erase_last_from(char* buffer) {
  int s = strlen(buffer);
  if (s) {
    buffer[s - 1] = 0;  //erase last character of pin
    s--;
    return 0;
  }
}

UserData get_user(int i) {
  UserData result;
  EEPROM.get(i * sizeof(UserData), result);
  return result;
}

void display_ready_message() {
  XC4630_box(0, 250, 239, 319, BLACK);
  XC4630_chara(0, 260, "ENTER PIN:", GREY, BLACK);
  XC4630_chara(0, 280, "OR SWIPE CARD.", GREY, BLACK);
}

void unlock() {
  pinMode(A5, OUTPUT); //These two lines trigger the relay open
  digitalWrite(A5, HIGH);
}

void lock() {
  digitalWrite(A5, LOW); // relay close
  pinMode(A5, INPUT);
}

char dopin(char* pin) {
  UserData user;
  for (int i = 0; i < USERCOUNT; i++) {
    user = get_user(i);

    if (strncmp(user.pin, pin, 8)) { continue; }
    if (!user.pin_allowed)         { break; }
    
    if (i==0) { 
      domaster(); 
    } else {
      dounlock(user.name);
    }
    pin[0] = 0;
    return 0;
  }

  doerror("PIN ERROR");
  pin[0] = 0;
  return 0;
}

char do_nothing(char* pin) {
  return 0;
}

void docard(byte* card_id) {
  UserData user;
  for (int i = 0; i < USERCOUNT; i++) {
    user = get_user(i);

    if (strncmp(user.card_id, card_id, 8)) { continue; }
    if (!user.card_allowed) { break; }

    if (i==0) { 
      domaster(); 
    } else {
      dounlock(user.name);
    }
    return;
  }
  
  doerror("CARD ERROR");
}

void dounlock(const char* name) {
  display_unlock_message(name);
  unlock();
  delay(RELAYTIME);
  lock();
  display_ready_message();
}

void display_unlock_message(const char* name) {
  XC4630_box(0, 250, 239, 319, BLACK);
  XC4630_chara(0, 260, "UNLOCK", GREEN, BLACK);
  XC4630_chara(0, 280, name, GREEN, BLACK);
}

void doerror(const char* reason) {
    XC4630_chara(0, 300, reason, RED, BLACK);  //error message
    delay(1000);
    XC4630_chara(0, 300, reason, BLACK, BLACK);
}

typedef struct Button {
  Point location;
  Size size;
  char caption[9];
} Button;

void Button_DrawButton(struct Button self) {
  XC4630_tbox(self.location.x, self.location.y, self.location.x + self.size.width, self.location.y + self.size.height, self.caption, WHITE, GREY, 1);
}

void draw(struct Button* buttons) {
  for(int i=0; i < 10; i++) {
    Button_DrawButton(buttons[i]);
  }
}

void domaster() {                                 //for master user to setup other users
  int u = 1;          //user to start editing
  byte done = 0;      //flag to say we've finished
  clear_screen();
  drawuserinfo(u);
  UserData user = get_user(u);
  static Button buttons[] = {
    (Button){ Point(165,65), Size(70,30), "EDIT" },
    (Button){ Point(5,145), Size(70,30), "DISABLE" },
    (Button){ Point(85,145), Size(70,30), "ENABLE" },
    (Button){ Point(165,145), Size(70,30), "EDIT" },
    (Button){ Point(5,225), Size(70,30), "DISABLE" },
    (Button){ Point(85,225), Size(70,30), "ENABLE" },
    (Button){ Point(165,225), Size(70,30), "EDIT" },
    (Button){ Point(5,265), Size(70,40), "PREVIOUS" },
    (Button){ Point(85,265), Size(70,40), "NEXT" },
    (Button){ Point(165,265), Size(70,40), "EXIT" },
  };
  draw(buttons);
  while (!done) {
    if (XC4630_istouch(165, 65, 235, 95)) {
      editusername(u);  //edit username
      drawuserinfo(u);
      delay(100);
    }
    if (XC4630_istouch(5, 145, 75, 175)) {
      user.card_allowed = 0;
      EEPROM.put(u * sizeof(UserData), user);
      drawuserinfo(u);
      delay(100);
    }
    if (XC4630_istouch(85, 145, 155, 175)) {
      user.card_allowed = 1;
      EEPROM.put(u * sizeof(UserData), user);
      drawuserinfo(u);
      delay(100);
    }
    if (XC4630_istouch(165, 145, 235, 175)) {
      editcard(u);  //edit card #
      drawuserinfo(u);
      delay(100);
    }
    if (XC4630_istouch(5, 225, 75, 255)) {
      user.pin_allowed = 0;
      EEPROM.put(u * sizeof(UserData), user);
      drawuserinfo(u);
      delay(100);
    }
    if (XC4630_istouch(85, 225, 155, 255)) {
      user.pin_allowed = 1;
      EEPROM.put(u * sizeof(UserData), user);
      drawuserinfo(u);
      delay(100);
    }
    if (XC4630_istouch(165, 225, 235, 255)) {
      editpin(u);  //edit PIN
      drawuserinfo(u);
      delay(100);
    }
    if (XC4630_istouch(5, 265, 75, 305)) {
      u = u - 1;  //previous
      if (u < 0) {
        u = USERCOUNT - 1;
      }
      clear_screen();
      drawuserinfo(u);
      user = get_user(u);
      delay(100);
    }
    if (XC4630_istouch(85, 265, 155, 305)) {
      u = u + 1;  //next
      if (u > USERCOUNT - 1) {
        u = 0;
      }
      clear_screen();
      drawuserinfo(u);
      user = get_user(u);
      delay(100);
    }
    if (XC4630_istouch(165, 265, 235, 305)) {
      done = 1; //exit
    }
  }
  draw(buttons);
  draw_user_screen();
}

void editusername(int u) {
  int done = 0;
  char c;
  int s;
  UserData user = get_user(u);
  for (int i = 0; i < 14; i++) {
    if (user.name[i] < 0) {
      user.name[i] = 0;
    }
  }
  user.name[13] = 0;
  clear_screen();
  XC4630_chara(0, 0, "TYPE USERNAME:", WHITE, BLACK);
  XC4630_tbox(5, 145, 115, 175, "CANCEL", WHITE, GREY, 2);
  XC4630_tbox(125, 145, 235, 175, "ACCEPT", WHITE, GREY, 2);
  drawkeyboard();
  while (!done) {
    s = strlen(user.name);
    XC4630_chara(0, 20, user.name, GREY, BLACK);
    XC4630_chara(s * 12, 20, "_ ", GREY * (((millis() / 300) & 1) != 0), BLACK);
    
    c = checkkeyboard();
    static const specialkey keys[] = {{'<',erase_last_from}};
    for(int i=0; i < 1; i++) {
      if (keys[i].key == c) {
        c = keys[i].function(user.name);
      }
    }

    s = strlen(user.name);
    if (c && s<13) {
      user.name[s] = c;  //add character
      user.name[s+1] = 0;
    }
    if (XC4630_istouch(5, 145, 115, 175)) {
      clear_screen();
      return;
    }
    if (XC4630_istouch(125, 145, 235, 175)) {
      for (int i = 0; i < 14; i++) {
        EEPROM.put(u * sizeof(UserData), user);
      }
      clear_screen();
      return;
    }
  }
}

void editcard(int u) {  //swipe new card- need to check if it matches an existing one before validating
  int umatch = -1;
  UserData user = get_user(u);

  clear_screen();
  getcard(user.card_id);
  for (int i = 0; i < USERCOUNT; i++) {
    UserData compare = get_user(i);
    if(strncmp(user.card_id, compare.card_id, 8)) { continue; }
    
    umatch = i; //flag matched user
    break;
  }
  
  clear_screen();

  if (umatch >= 0) {
    XC4630_chara(0, 150, "CARD ALREADY IN USE!", RED, BLACK);
    delay(2000);
    clear_screen();
    return;
  }
  
  EEPROM.put(u * sizeof(UserData), user);
}

void editpin(int u) {   //enter new PIN- need to check if it matches an existing one before validating
  int umatch = -1;
  UserData user = get_user(u);
  
  clear_screen();
  getpin(user.pin);
  for (int i = 0; i < USERCOUNT; i++) {
    UserData compare = get_user(i);
    if(strncmp(user.pin, compare.pin, 8)) { continue; }

    umatch = i;
    break;
  }
  
  clear_screen();
  
  if (umatch >= 0) {
    XC4630_chara(8, 150, "PIN ALREADY IN USE!", RED, BLACK);
    delay(2000);
    clear_screen();
    return;
  }
  
  EEPROM.put(u * sizeof(UserData), user);
}

int is_set(const byte* field) {
  int e = 0;
  for(int i=0; i < 8; i++) { e += field[i]; }
  return e != 8 * 0xFF;
}

void drawuserinfo(int u) {
  UserData user = get_user(u);
  XC4630_chara(0, 0, "USER INFO:", WHITE, BLACK);
  XC4630_char(120, 0, (u / 10) % 10 + '0', WHITE, BLACK);
  XC4630_char(132, 0, (u) % 10 + '0', WHITE, BLACK);
  XC4630_chara(0, 20, "Name:", WHITE, BLACK);
  XC4630_chara(0, 40, user.name, WHITE, BLACK);
  
  XC4630_chara(0, 100, "Card:", WHITE, BLACK);
  if (is_set(user.card_id)) {
    XC4630_chara(0, 120, "SET    ", GREEN, BLACK);
  } else {
    XC4630_chara(0, 120, "NOT SET", RED, BLACK);
  }
  if (user.card_allowed) {
    XC4630_chara(120, 120, "ACTIVE  ", GREEN, BLACK);
  } else {
    XC4630_chara(120, 120, "DISABLED", RED, BLACK);
  }
  
  XC4630_chara(0, 180, "PIN:", WHITE, BLACK);
  if (is_set(user.pin)) {
    XC4630_chara(0, 200, "SET    ", GREEN, BLACK);
  } else {
    XC4630_chara(0, 200, "NOT SET", RED, BLACK);
  }
  if (user.pin_allowed) {
    XC4630_chara(120, 200, "ACTIVE  ", GREEN, BLACK);
  } else {
    XC4630_chara(120, 200, "DISABLED", RED, BLACK);
  }
}

void dosetup(UserData admin) {
  strncpy(admin.name, "MASTER USER\0", 13);

  clear_screen();
  XC4630_chara(0, 0, " MASTER USER SETUP  ", WHITE, RED_1 * 8);                   //warning for master setup
  admin.card_allowed = getcard(admin.card_id);                                                            //get a card, returns 0 if no card selected

  clear_screen();
  XC4630_chara(0, 0, " MASTER USER SETUP  ", WHITE, RED_1 * 8);                   //warning for master setup
  admin.pin_allowed = getpin(admin.pin);                                                              //get a pin, returns 0 if no pin entered/cancelled
  
  EEPROM.put(0,admin);
}

byte getcard(byte* result) {      //get a swiped card for setup
  static const char hex[] = "0123456789ABCDEF";
  byte done = 0;
  byte cardset = 0;
  for (int i = 0; i < 8; i++) {
    result[i] = 0;
  }
  XC4630_chara(8, 30, "Swipe a card to set", GREY, BLACK);
  XC4630_tbox(125, 90, 235, 120, "CANCEL", RED, GREY, 2);
  byte card_id[8];
  while (!done) {
    if (checkcard(card_id)) {        //valid card swiped
      for (int i = 0; i < 8; i++) {
        result[i] = card_id[i];
        XC4630_char(i * 30 + 4, 60, hex[result[i] >> 4], WHITE, BLACK);
        XC4630_char(i * 30 + 16, 60, hex[result[i] & 15], WHITE, BLACK);
      }
      cardset = 1;
      XC4630_tbox(5, 90, 115, 120, "USE CARD", WHITE, GREY, 2);
    }
    if (XC4630_istouch(5, 90, 115, 120) && cardset) {
      done = 1; //keep card value
    }
    if (XC4630_istouch(125, 90, 235, 120)) {
      for (int i = 0; i < 8; i++) {
        result[i] = 0; //clear card value
      }
      done = 1;
      cardset = 0;
    }
  }
  return cardset;
}

byte getpin(char* pin) {       //get a typed pin for setup
  byte done = 0;
  byte pinset = 0;
  draw_pinpad();
  XC4630_chara(0, 260, "ENTER PIN:", WHITE, BLACK);
  XC4630_tbox(125, 289, 235, 319, "CANCEL", RED, GREY, 2);
  for (int i = 0; i < 8; i++) {
    pin[i] = 0;
  }
  while (!done) {
    char a;
    int s;
    a = checktouch();
    
    static const specialkey keys[] = {{'#',erase_last_from}, {'*', do_nothing}};
    for(int i=0; i < 2; i++) {
      if (keys[i].key == a) {
        a = keys[i].function(pin);
      }
    }
    
    if (a) {
      s = strlen(pin);
      pin[s] = a;
    }
    for (int i = 0; i < 8; i++) {
      XC4630_char(120 + i * 12, 260, pin[i], GREY, BLACK);
    }

    pinset = strlen(pin) >= 4;
    XC4630_tbox(5, 289, 115, 319, "USE PIN", pinset * WHITE, pinset * GREY, 2);
    if (XC4630_istouch(5, 289, 115, 319) && pinset) {
      done = 1; //keep pin
    }
    if (XC4630_istouch(125, 289, 235, 319)) {
      pinset = pin[0] = 0;
      done = 1;
    }
  }
  return pinset;
}

void drawkeyboard() {
  char t[] = " ";
  for (int i = 0; i < 40; i++) {
    t[0] = kb[i];
    XC4630_tbox((i % 10) * 24, 200 + (i / 10) * 30, (i % 10) * 24 + 22, 228 + (i / 10) * 30, t, BLACK, GREY, 3);
  }
}

char checkkeyboard() {
  int x, y;
  static char lastn = 0;
  char n;
  n = 0;
  x = XC4630_touchx();
  y = XC4630_touchy();
  if ((x > 0) && (y > 200)) {
    n = kb[x / 24 + ((y - 200) / 30) * 10];
  }
  if ((lastn == 0) && (n)) {
    lastn = n;
    return n;
  }
  lastn = n;
  return 0;
}

int checkcard(byte* result) {
  for (byte i = 0; i < 8; i++) {
    result[i] = 0;
  }
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return 0; //no card found
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return 0; //no card found
  }
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (i < 8) { // FIXME: only pull the first 8 bytes (so, the loop only matters if uid.size < 8)
      result[i] = mfrc522.uid.uidByte[i];
    }
  }
  mfrc522.PICC_HaltA();
  return 1;                                             //card ID read
}

void draw_pinpad() {
  for (int i = 0; i < 12; i++) {
    dobutton(i, WHITE, GREY, 50);
  }
}

void dobutton(int n, unsigned int f, unsigned int b, int s) {
  char t[] = " "; //single char array
  t[0] = bb[n];
  XC4630_tbox(bx[n], by[n], bx[n] + s - 1, by[n] + s - 1, t, f, b, 6);
}

char checktouch() {                    //returns touched button
  static byte ltstate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  byte tstate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  char key = 0;
  for (int i = 0; i < 12; i++) {
    tstate[i] = XC4630_istouch(bx[i], by[i], bx[i] + 59, by[i] + 59);
    delay(5);
    tstate[i] = XC4630_istouch(bx[i], by[i], bx[i] + 59, by[i] + 59)&tstate[i]; //debounce
    if (tstate[i] != ltstate[i]) {
      if (tstate[i]) {
        dobutton(i, BLACK, WHITE, 50);
        key = bb[i];                                                       //key pressed
      } else {
        dobutton(i, WHITE, GREY, 50);
      }
    }
    ltstate[i] = tstate[i];
  }
  return key;
}
