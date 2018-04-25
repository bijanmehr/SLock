#include <EEPROM.h>  .// از طریق کتابخانه EEPROM آی دی یونیک کارت و تگ های RFID را میخوانیم
#include <SPI.h>      // ماژول RC522 از پروتکل SPI استفاده میکند.
#include <MFRC522.h>   // کتابخانه مورد استفاده برای ماژول های MFRC522

#include <Servo.h>
#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#define redLed 7
#define greenLed 6
#define blueLed 5
#define relay 4

boolean match = false; // مقدار دهی اولیه کارت 
boolean programMode = false; //مقدار دهی اولیه شروع برنامه نویسی
int successRead; // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4];   // آی دی خوانده شده از EEPROM ذخیره میشود
byte readCard[4];           // آی دی های اسکن شده از ماژولRFID را ذخیره میشود.
byte masterCard[4]; // آی دی کارت ارشد خوانده شده از EEPROM ذخیره میشود

/* We need to define MFRC522's pins and create instance
 * Pin layout should be as follows (on Arduino Uno):
 * MOSI: Pin 11 / ICSP-4
 * MISO: Pin 12 / ICSP-1
 * SCK : Pin 13 / ICSP-3
 * SS : Pin 10 (Configurable)
 * RST : Pin 9 (Configurable)
 * look MFRC522 Library for
 * pin configuration for other Arduinos.
 */

#define SS_PIN D8
#define RST_PIN D0
MFRC522 mfrc522(SS_PIN, RST_PIN);  // ساخت MFRC522

///////////////////////////////////////// تنظیمات///////////////////////////
void setup() {
  //Arduino Pin Configuration
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(redLed, LED_OFF); //مطمئن شوید که ال ای دی خاموش است
  digitalWrite(greenLed, LED_OFF); //مطمئن شوید که ال ای دی خاموش است
  digitalWrite(blueLed, LED_OFF); //مطمئن شوید که ال ای دی خاموش است
  
  //پیکربندی پروتکل
  Serial.begin(115200);  // ارتباطات سریال با pc تنظیم روی باند سریال 96000
  SPI.begin();           // سخت افزار MFRC522 از پروتکل SPI استفاده میکند.
  mfrc522.PCD_Init();    // مقدار دهی اولیه سخت افزار MFRC522
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max); //ست کردن آنتن برای ماژول RFID جهت افزایش برد گیرنده که در این آموزش به این موضوع نمیپردازیم.

  
  //چک کنید اگر کارت ارشد انتخاب شده است، به کاربر اجازه انتخاب و تعریف کارت ارشد را ندهید
  //این قسمت تنها برای تعریف مجدد کارت ارشد مناسب است.
  if (EEPROM.read(1) != 143) {  // بررسی حافظه EEPROM برای شناخت مستر کارت
    Serial.println("No Master Card Defined");
    Serial.println("Scan A PICC to Define as Master Card");
    do {
      successRead = getID();
      digitalWrite(blueLed, LED_ON); // هنکامی که به مستر کارت نیاز است، با ال ای دی آبی نمایش میدهیم.
      delay(200);
      digitalWrite(blueLed, LED_OFF);
      delay(200);
    }
    while (!successRead); //در این مرحله اگر کارت خوانده نشود، نمیتوانید به مرحله بعد بروید.
    for ( int j = 0; j < 4; j++ ) { // این حلقه 4 بار تکرار خواهد شد.
      EEPROM.write( 2 +j, readCard[j] ); //آی دی یونیک کارت ارشد در EEPROM نوشته میشود. از آدرس 3 شروع میشود.
    }
    EEPROM.write(1,143); //کارت ارشد که از قبل تعریف کرده ایم در EEPROM رایت میشود.
    Serial.println("Master Card Defined");
  }
  Serial.println("Master Card's UID");
  for ( int i = 0; i < 4; i++ ) {     // خواندن آی دی منحصربه فرد کارت ارشد از حافظه EEPROM
    masterCard[i] = EEPROM.read(2+i); // رایت آن در کارت ارشد
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println("Waiting PICCs to bo scanned :)");
  cycleLeds();    // در این مرحله با تغییر وضعیت ال ای دی ها فیدبکی از عملکرد RFID به کاربر داده خواهد شد.
}


///////////////////////////////////////// حلقه اصلی ///////////////////////////////////
void loop () {
  do {
    successRead = getID(); // کد به درستی خوانده شده و به متغیر 1 تغییر داده میشود و در غیر این صورت 0 است.
    if (programMode) {
      cycleLeds(); // در صورتی که آی دی کارت به درستی خوانده شود ، ال ا ی دی ها در حلقه تکرار قرار گرفته و تا زمانی که کارت جدید تعریف شوند تغییر حالت میدهند.
    }
    else {
      normalModeOn(); // در غیر اینصورت تنها ال ای دی آبی روشن و دیگر ال ای دی ها خاموش است.
    }
  }
  while (!successRead);
  if (programMode) {
    if ( isMaster(readCard) ) {  //اگر کارت ارشد تعریف شده باشد به منو اصلی باز میگردد
      Serial.println("This is Master Card"); 
      Serial.println("Exiting Program Mode");
      Serial.println("-----------------------------");
      programMode = false;
      return;
    }
    else {  
      if ( findID(readCard) ) { //اگر کارت اسکن شده شناخته شده باشد آن را حذف میکند.
        Serial.println("I know this PICC, so removing");
        deleteID(readCard);
        Serial.println("-----------------------------");
      }
      else {                    // و اگر کارت اسکن شده ناشناخته باشد، آن را اضافه خواهد کرد.
        Serial.println("I do not know this PICC, adding...");
        writeID(readCard);
        Serial.println("-----------------------------");
      }
    }
  }
  else {
    if ( isMaster(readCard) ) {  // اگر آی دی کارت اسکن شده با آی دی کارت ارشد تطبیق داده شودوارد حالت تنظیم برنامه خواهد شد.
      programMode = true;
      Serial.println("Hello Master - Entered Program Mode");
      int count = EEPROM.read(0); // اولین بایت از حافظه EEPROM خوانده میشود
      Serial.print("I have ");    // و شماره آی دی در EEPROM ذخیره میشود
      Serial.print(count);
      Serial.print(" record(s) on EEPROM");
      Serial.println("");
      Serial.println("Scan a PICC to ADD or REMOVE");
      Serial.println("-----------------------------");
    }
    else {
      if ( findID(readCard) ) {        // اگر آی دی شناسایی شود پیغام ورود برای شماارسال میگردد
        Serial.println("Welcome, You shall pass");
      }
      else {        //در غیر اینصورت پیغامی مبنی بر عدم اجازه ورود نمایش داده خواهد شد.
        Serial.println("You shall not pass");
        failed(); 
      }
    }
  }
}

///////////////////////////////////////// دریافت آی دی منحصر به فرد ///////////////////////////////////
int getID() {
  // آماده برای خواندن PICC 
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //اگر یک PICC جدید به RFID معرفی شد، ادامه میدهد.
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }
  // دو مدل کارت مایفر وجود دارند، 4 بایتی و 7 بایتی، اگر از مدل 7 بایتی 
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println("Scanned PICC's UID:");
  for (int i = 0; i < 4; i++) {  // 
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // توقف خواندن کارت
  return 1;
}

///////////////////////////////////////// حلقه ال ای دی ها (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(redLed, LED_OFF); // مطمئن شوید که ال ای دی قرمز خاموش است
  digitalWrite(greenLed, LED_ON); // مطمئن شوید که ال ای دی سبز روشن است
  digitalWrite(blueLed, LED_OFF); // مطمئن شوید که ال ای دی آبی 
  delay(200);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_ON);
  delay(200);
  digitalWrite(redLed, LED_ON);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_OFF);
  delay(200);
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(blueLed, LED_ON); // ال ای دی آبی روشن است و آماده برای خواندن کارت است
  digitalWrite(redLed, LED_OFF); // مطمئن شوید که ال ای دی قرمز خاموش است
  digitalWrite(greenLed, LED_OFF); // مطمئن شوید که ال ای دی سبز خاموش است
  }

////////////////////////////////////////خواندن ID از EEPROM //////////////////////////////
void readID( int number ) {
  int start = (number * 4 ) + 2; // تشخیص موقعیت شروع
  for ( int i = 0; i < 4; i++ ) { // حلقه 4 بار تکرار میشود تا 4 بایت دریافت کند
    storedCard[i] = EEPROM.read(start+i); // مقادیر خوانده شده از EEPROM را به آرایه اختصاص دهید
  }
}

///////////////////////////////////////// اضافه کردن آی دی به EEPROM  ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) { // قبل از رایت چک میشود تا قبلا این کارت در حافظه EEPROM رایت نشده باشد.
    int num = EEPROM.read(0); // مقدار 0 مقادیر آی دی را ذخیره میکند.
    int start = ( num * 4 ) + 6;
    num++;
    EEPROM.write( 0, num );
    for ( int j = 0; j < 4; j++ ) {
      EEPROM.write( start+j, a[j] );
    }
    successWrite();
  }
  else {
    failedWrite();
  }
}

///////////////////////////////////////// حذف آی دی از EEPROM  ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) { // قبل از حذف یک کارت از حافظه EEPROM چک شود که قبلا ذخیره بوده است یا نه!
    failedWrite();
  }
  else {
    int num = EEPROM.read(0);
    int slot; // 
    int start;// = ( num * 4 ) + 6;
    int looping;
    int j;
    int count = EEPROM.read(0);
    slot = findIDSLOT( a );
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;
    EEPROM.write( 0, num );
    for ( j = 0; j < looping; j++ ) {
      EEPROM.write( start+j, EEPROM.read(start+4+j));
    }
    for ( int k = 0; k < 4; k++ ) {
      EEPROM.write( start+j+k, 0);
    }
    successDelete();
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != NULL )
    match = true;
  for ( int k = 0; k < 4; k++ ) { // چرخه به مدت 4 بار تکرار شود.
    if ( a[k] != b[k] )
      match = false;
  }
  if ( match ) {
    return true; // ارسال پاسخ true
  }
  else  {
    return false; // ارسال پاسخ false
  }
}

///////////////////////////////////////// پیدا کردن SLOT  ///////////////////////////////////
int findIDSLOT( byte find[] ) {
  int count = EEPROM.read(0); // خواندن اولین بایت EEPROM
  for ( int i = 1; i <= count; i++ ) {
    readID(i); // خواندن آیدی از EEPROM
    if( checkTwo( find, storedCard ) ) { // بررسی خواندن کارت از حافظه EEPROM
      // اگر با آیدی کارت یکسان بود اجازه عبور صادر شود.
      return i; // شماره Slot کارت
      break;
    }
  }
}

///////////////////////////////////////// پیدا کردن آی دی از EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  int count = EEPROM.read(0);
  for ( int i = 1; i <= count; i++ ) { 
    readID(i);
    if( checkTwo( find, storedCard ) ) {
      return true;
      break;
    }
    else { 
    }
  }
  return false;
}

///////////////////////////////////////// رایت در EEPROM   ///////////////////////////////////
// ال ای دی سبز 3 مرتبه به این معنی که رایت با موفقیت انجام شده است، چشمک میزند.
void successWrite() {
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF); 
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(greenLed, LED_ON);
  delay(200);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(greenLed, LED_ON);
  delay(200);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(greenLed, LED_ON);
  delay(200);
  Serial.println("Succesfully added ID record to EEPROM");
}

////////////////

///////////////////////// رایت ناموفق در EEPROM   ///////////////////////////////////
// ال ای دی قرمز 3 مرتبه به این معنی که رایت انحام نشده است، چشمک میزند.
void failedWrite() {
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(redLed, LED_ON);
  delay(200);
  digitalWrite(redLed, LED_OFF); 
  delay(200);
  digitalWrite(redLed, LED_ON);
  delay(200);
  digitalWrite(redLed, LED_OFF);
  delay(200);
  digitalWrite(redLed, LED_ON);
  delay(200);
  Serial.println("Failed! There is something wrong with ID or bad EEPROM");
}

///////////////////////////////////////// حذف آی دی از EEPROM ///////////////////////////////////
// ال ای دی آبی 3 مرتبه به این معنی که آی دی از EEPROM حذف شده است، چشمک میزند.
void successDelete() {
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(blueLed, LED_ON);
  delay(200);
  digitalWrite(blueLed, LED_OFF);
  delay(200);
  digitalWrite(blueLed, LED_ON);
  delay(200);
  digitalWrite(blueLed, LED_OFF);
  delay(200);
  digitalWrite(blueLed, LED_ON); //
  delay(200);
  Serial.println("Succesfully removed ID record from EEPROM");
}

////////////////////// چک کردن کارت ارشد  ///////////////////////////////////
// در این مرحله چک میکند که اگر آی دی مورد قبول است ،به عنوان کارت ارشد شناخته میشود.
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

/////////////////////////////////////////عدم دسترسی  ///////////////////////////////////
void failed() {
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_ON);
  delay(1200);
}
