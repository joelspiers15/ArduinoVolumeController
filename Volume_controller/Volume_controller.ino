/***********************************************************************************
  This program is a demo of drawing
  This demo was made for LCD modules with 8bit or 16bit data port.
  This program requires the the LCDKIWI library.

  File                : touch_pen.ino
  Hardware Environment: Arduino UNO&Mega2560
  Build Environment   : Arduino

  Set the pins to the correct ones for your development shield or breakout board.
  This demo use the BREAKOUT BOARD only and use these 8bit data lines to the LCD,
  pin usage as follow:
                   LCD_CS  LCD_CD  LCD_WR  LCD_RD  LCD_RST  SD_SS  SD_DI  SD_DO  SD_SCK
      Arduino Uno    A3      A2      A1      A0      A4      10     11     12      13
  Arduino Mega2560    A3      A2      A1      A0      A4      10     11     12      13

                   LCD_D0  LCD_D1  LCD_D2  LCD_D3  LCD_D4  LCD_D5  LCD_D6  LCD_D7
      Arduino Uno    8       9       2       3       4       5       6       7
  Arduino Mega2560    8       9       2       3       4       5       6       7

  Remember to set the pins to suit your display module!

  @attention

  THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  TIME. AS A RESULT, QD electronic SHALL NOT BE HELD LIABLE FOR ANY
  DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
**********************************************************************************/

#include <TouchScreen.h> //touch library
#include <LCDWIKI_GUI.h> //Core graphics library
#include <LCDWIKI_KBV.h> //Hardware-specific library

#include <avr/pgmspace.h>
#include <ArduinoJson.h>

//if the IC model is known or the modules is unreadable,you can use this constructed function
LCDWIKI_KBV screen(ILI9486, A3, A2, A1, A0, A4); //model,cs,cd,wr,rd,reset
//if the IC model is not known and the modules is readable,you can use this constructed function
//LCDWIKI_KBV screen(320,480,A3,A2,A1,A0,A4);//width,height,cs,cd,wr,rd,reset

#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

//param calibration from kbv
#define TS_MINX 906
#define TS_MAXX 116

#define TS_MINY 92
#define TS_MAXY 952

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// Colors
#define  BLACK   0x0000
#define DARK_GRAY 0X0841
#define GRAY    0x2124
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define SPOTIFY_GREEN 0x1ED760

// Main UI params
#define SECTION_HEIGHT screen.Get_Display_Height()/2
#define SECTION_WIDTH screen.Get_Display_Width()/2
#define ICON_SIZE 128
#define ICON_PADDING ((SECTION_WIDTH-ICON_SIZE)/2)
#define VOLUME_HEIGHT 32

// Touch menu params
#define BUTTON_HEIGHT (screen.Get_Display_Height()/4)
#define BUTTON_WIDTH (screen.Get_Display_Width()/1.5)
#define BUTTON_X_PADDING ((screen.Get_Display_Width() - BUTTON_WIDTH)/2)
#define BUTTON_Y_PADDING ((screen.Get_Display_Height() - BUTTON_HEIGHT)/12)

bool menuOpen = false;
bool asleep = false;

int16_t xOrigin[] = {0, SECTION_WIDTH, 0, SECTION_WIDTH};
int16_t yOrigin[] = {0, 0, SECTION_HEIGHT, SECTION_HEIGHT};
String title[] = {"a", "a", "a", "a"};
uint8_t volume[] = {25, 50, 75, 100};
uint16_t color[] = {0x9874, BLUE, YELLOW, RED};
uint8_t potPin[] = {15, 14, 13, 12};
uint16_t icon[] = {NULL, NULL, NULL, NULL};
bool active[] = {false, false, false, false};
bool iconNeedsUpdate[] = {false, false, false, false};

// Serial input variables
const byte numChars = 256;
char receivedChars[numChars];
bool newData = false;

void render_dividers(void)
{
  screen.Set_Draw_color(WHITE);
  screen.Draw_Fast_VLine(screen.Get_Display_Width() / 2, 0, screen.Get_Display_Height());
  screen.Draw_Fast_VLine((screen.Get_Display_Width() / 2) - 1, 0, screen.Get_Display_Height());
  screen.Draw_Fast_HLine(0, screen.Get_Display_Height() / 2, screen.Get_Display_Width());
  screen.Draw_Fast_HLine(0, (screen.Get_Display_Height() / 2) - 1, screen.Get_Display_Width());
}

// Render just the text for a section
void render_volume(int section) {
  int width = map(volume[section], 0, 100, 0, SECTION_WIDTH);
  screen.Fill_Rect(
    xOrigin[section],
    yOrigin[section] + SECTION_HEIGHT - VOLUME_HEIGHT - 1,
    width,
    VOLUME_HEIGHT,
    color[section]
  );

  screen.Fill_Rect(
    xOrigin[section] + width,
    yOrigin[section] + SECTION_HEIGHT - VOLUME_HEIGHT - 1,
    SECTION_WIDTH - width,
    VOLUME_HEIGHT,
    DARK_GRAY
  );
}

// Render just the text for a section
void render_text(int section) {
  screen.Fill_Rect(
    xOrigin[section] + ICON_PADDING,
    yOrigin[section] + (ICON_PADDING * 2) + ICON_SIZE,
    SECTION_WIDTH,
    VOLUME_HEIGHT,
    BLACK
    );
  screen.Print_String(
    title[section].substring(0,11),
    xOrigin[section] + ICON_PADDING,
    yOrigin[section] + (ICON_PADDING * 2) + ICON_SIZE
  );
}

void render_icon(int section) {
  int16_t x = xOrigin[section] + ICON_PADDING;
  int16_t y = yOrigin[section] + ICON_PADDING;

//  screen.Fill_Rect(
//    x,
//    y,
//    ICON_SIZE,
//    ICON_SIZE,
//    WHITE
//  );

  //screen.Print_String(" Loading ", x + ICON_SIZE/16, y + ICON_SIZE*6/8);
  //screen.Print_String("  icon   ", x + ICON_SIZE/16, y + ICON_SIZE*7/8);

  Serial.println("{\"type\": \"icon_request\", \"index\": " + String(section) + "}");
  for (int yPos = 0; yPos < ICON_SIZE; yPos++) {
    for (int xPos = 0; xPos < ICON_SIZE; xPos++) {
      while(!Serial.available()) {
        delay(10);
      }
      byte colorByte[2];
      (uint16_t)Serial.readBytes(colorByte, 2);
//      Serial.print("0x");
//      Serial.println(color, HEX);
      uint16_t color = word(colorByte[0], colorByte[1]);
      screen.Set_Draw_color(color);
      screen.Draw_Pixel(xPos + x, yPos + y);
    }
  }
  iconNeedsUpdate[section] = false;
}

// Render a full section (icon, title, volume)
void render_section(int section)
{
  // Render title
  render_text(section);

  // Render volume bar
  render_volume(section);

  // Render Icon
  //render_icon(section);
}

void clear_section(int section) {
  screen.Fill_Rect(xOrigin[section], yOrigin[section], SECTION_WIDTH, SECTION_HEIGHT, BLACK);
}

// Render all sections
void render_full(void)
{
  
  //render_dividers();
  screen.Fill_Screen(BLACK);

  for (int i = 0; i < 4; i++) {
    if(active[i]) {
      render_section(i);
    }
  }
}

void render_menu(void)
{
  screen.Fill_Screen(BLACK);

  //Button 1
  screen.Set_Draw_color(WHITE);
  screen.Draw_Rectangle(
    BUTTON_X_PADDING, 
    BUTTON_Y_PADDING, 
    BUTTON_X_PADDING + BUTTON_WIDTH, 
    BUTTON_Y_PADDING + BUTTON_HEIGHT
  );
  screen.Print_String(
    "Sleep",
    BUTTON_X_PADDING + 60, 
    BUTTON_Y_PADDING + 50
  );

  //Button 2
  screen.Fill_Rect(
    BUTTON_X_PADDING, 
    (BUTTON_Y_PADDING * 2) + BUTTON_HEIGHT, 
    BUTTON_WIDTH, 
    BUTTON_HEIGHT,
    BLUE
  );
  screen.Print_String(
    "Other",
    BUTTON_X_PADDING + 60, 
    BUTTON_Y_PADDING * 2 + BUTTON_HEIGHT + 50
  );

  //Button 3
  screen.Fill_Rect(
    BUTTON_X_PADDING,
    (BUTTON_Y_PADDING * 3) + (BUTTON_HEIGHT * 2),
    BUTTON_WIDTH,
    BUTTON_HEIGHT,
    RED
  );
  screen.Print_String(
    "Exit Menu",
    BUTTON_X_PADDING + 30, 
    BUTTON_Y_PADDING*3 + BUTTON_HEIGHT*2 + 50
  );
}

void render_error(String message) {
  screen.Fill_Screen(BLACK);
  screen.Print_String("ERROR", 0,0);
  screen.Print_String(message, 0, 50);
}

void render_waiting_on_serial() {
  screen.Print_String("Waiting on", 0, 0);
  screen.Print_String("Serial connection", 0, 50);
}

void setup(void)
{
  Serial.begin(57600);
  screen.Init_LCD();
  screen.Set_Rotation(2);
  screen.Fill_Screen(BLACK);

  // Setup graphics engine defaults
  screen.Set_Draw_color(WHITE);
  screen.Set_Text_Back_colour(BLACK);
  screen.Set_Text_Mode(0);
  screen.Set_Text_Size(2);
  screen.Set_Text_colour(WHITE);

  bool waiting = false;
  while(!Serial) {
    delay(500);
    if(!waiting && !Serial) {
      render_waiting_on_serial();
      waiting = true;
    }
  }
  screen.Fill_Screen(BLACK);

  //Read from test json
  checkForSerialCommand();

  //render_full();
//  for(int i = 0; i < 4; i++) {
//    render_icon(i);
//  }
  

  pinMode(13, OUTPUT);
}

#define MINPRESSURE 10
#define MAXPRESSURE 1000

void loop()
{
  if(!menuOpen && !asleep) {
    // Check for volume updates
    int newVolume[4];
    for (int i = 0; i < 4; i++) {
      newVolume[i] = map(analogRead(potPin[i]), 1024, 0, 0, 100);
    }
    for (int i = 0; i < 4; i++) {
      if (newVolume[i] != volume[i] && active[i]) {
        volume[i] = newVolume[i];
        render_volume(i);
        sendVolumeChangeRequest(i, volume[i]);
      }
    }

    for(int i = 0; i < 4; i++){
      if(iconNeedsUpdate[i] && active[i]){
        render_icon(i);
      }
    }
  }
  
  if(!checkForSerialCommand()) {
    render_error("Serial read failed");
  }

  // Do screen touching stuff
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE)
  {
    Serial.println("[" + String(p.x) + "," + String(p.y) + "]");
    int x = map(p.x, 0, 1000, 0, screen.Get_Display_Width());
    int y = map(p.y, 0, 1000, 0, screen.Get_Display_Height());
    
    // Screen touched
    if(!menuOpen){
      menuOpen = true;
      asleep = false;
      render_menu();
      for(int i = 0; i < sizeof(iconNeedsUpdate); i++) {
        iconNeedsUpdate[i] = true;
      }
    } else {
      if(y < screen.Get_Display_Height()/3) {
        // Turn off display button pressed
        menuOpen = false;
        asleep = true;
        screen.Fill_Screen(BLACK);
      } else if (y >= screen.Get_Display_Height()/3 && y < (screen.Get_Display_Height() * 2/3)) {
        // Button #2
      } else if (y >= (screen.Get_Display_Height() * 2/3)) {
        // Close menu button pressed
        menuOpen = false;
        render_full();
      }
    }
  }
}

void sendVolumeChangeRequest(int section, int volume) {
  Serial.println("{\"type\": \"volume_change\", \"index\": " + String(section) + ", \"volume\": " + String(volume) + "}");
}

bool checkForSerialCommand(){
  //Read from test json
//  DeserializationError err = deserializeJson(jsonDoc, Serial);
//  if(err) {
//    Serial.print("ERROR:");
//    Serial.println(err.c_str());
//  }
  if(Serial.available()){
    DynamicJsonBuffer jb;
    JsonObject& root = jb.parseObject(Serial);
  
    
    const char* type = root["type"];
    if(strcmp(type, "data") == 0) {

      // Loop through all apps and update any that have changed
      for(int i = 0; i < root["size"]; i++) {
        active[i] = true;
        const char* titleChar = (char*)root["applications"][i]["title"];
        String newTitle = titleChar;

        // If the title of the current section has changed redraw everything
        if(sizeof(newTitle) > 0 && newTitle.compareTo(title[i]) != 0) {
          Serial.println("Updating title: " + title[i] + " -> " + newTitle);
          title[i] = newTitle;
          color[i] = root["applications"][i]["color"];
          iconNeedsUpdate[i] = true;
          clear_section(i);
          render_text(i);
          render_volume(i);
        }
      }

      for(int i = 3; i >= root["size"]; i--)
      {
        active[i] = false;
        clear_section(i);
      }
    }
  }
  return true;
}
