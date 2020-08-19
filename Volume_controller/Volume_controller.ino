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

int16_t xOrigin[] = {0, SECTION_WIDTH, 0, SECTION_WIDTH};
int16_t yOrigin[] = {0, 0, SECTION_HEIGHT, SECTION_HEIGHT};
String title[] = {"", "", "", ""};
uint8_t volume[] = {25, 50, 75, 100};
uint16_t color[] = {0x9874, BLUE, YELLOW, RED};
uint8_t potPin[] = {15, 14, 13, 12};
uint16_t icon[] = {NULL, NULL, NULL, NULL};

// Serial input variables
const byte numChars = 256;
char receivedChars[numChars];
bool newData = false;

const char* testJsonInput = 
  "{\"type\": \"data\",\"applications\": [{\"Title\": \"Spotify\",\"Volume\": 100,\"Color\": 2016},{\"Title\": \"Chrome\",\"Volume\": 100,\"Color\": 31},{\"Title\": \"Discord\",\"Volume\": 100,\"Color\": 65504},{\"Title\": \"Apex Legends\",\"Volume\": 100,\"Color\": 63488}]}";

StaticJsonDocument<256> jsonDoc;

void render_dividers(void)
{
  screen.Set_Draw_color(WHITE);
  screen.Draw_Fast_VLine(screen.Get_Display_Width() / 2, 0, screen.Get_Display_Height());
  screen.Draw_Fast_VLine((screen.Get_Display_Width() / 2) - 1, 0, screen.Get_Display_Height());
  screen.Draw_Fast_HLine(0, screen.Get_Display_Height() / 2, screen.Get_Display_Width());
  screen.Draw_Fast_HLine(0, (screen.Get_Display_Height() / 2) - 1, screen.Get_Display_Width());
}

// Render just the volume bar
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

void render_error(String message) {
  //Nothing for now
}

void render_icon(int16_t x, int16_t y, uint16_t *data) {

  screen.Draw_Rectangle(
    x,
    y,
    x + ICON_SIZE,
    y + ICON_SIZE
  );

//  for (int yPos = 0; yPos < ICON_SIZE; yPos++) {
//    for (int xPos = 0; xPos < ICON_SIZE; xPos++) {
//      //      screen.Set_Draw_color(WHITE);
//      //      screen.Draw_Pixel(xPos + x, yPos + y);
//      uint16_t color = pgm_read_word_near(data + xPos + (yPos * ICON_SIZE));
//      //      screen.Fill_Rect(0, 200, screen.Get_Display_Height(), 30, BLACK);
//      //      screen.Print_String("0x" + String(color, HEX) + "\t i=" + String(xPos+yPos), 0, 200);
//      //      delay(250);
//      screen.Set_Draw_color(color);
//      screen.Draw_Pixel(xPos + x, yPos + y);
//    }
//    //    screen.Fill_Rect(0, 200, screen.Get_Display_Height(), 30, BLACK);
//    //    screen.Print_String("NEW LINE", 0, 200);
//    //    delay(250);
//  }
}

// Render a full section (icon, title, volume)
void render_section(int section)
{
  // Render title
  screen.Print_String(
    title[section],
    xOrigin[section] + ICON_PADDING,
    yOrigin[section] + (ICON_PADDING * 2) + ICON_SIZE
  );

  // Render volume bar
  render_volume(section);

  // Render Icon
  render_icon(
    xOrigin[section] + ICON_PADDING,
    yOrigin[section] + ICON_PADDING,
    icon[section]
  );
}

// Render all sections
void render_full(void)
{
  screen.Fill_Screen(BLACK);
  
  //render_dividers();

  for (int i = 0; i < 4; i++) {
    render_section(i);
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

void setup(void)
{
  Serial.begin(9600);
  screen.Init_LCD();
  screen.Set_Rotation(2);
  Serial.println(screen.Read_ID(), HEX);
  screen.Fill_Screen(BLACK);

  // Setup graphics engine defaults
  screen.Set_Draw_color(WHITE);
  screen.Set_Text_Mode(1);
  screen.Set_Text_Size(3);
  screen.Set_Text_colour(WHITE);

  //Read from test json
  DeserializationError err = deserializeJson(jsonDoc, testJsonInput);
  if(err) {
    Serial.print("ERROR:");
    Serial.println(err.c_str());
  }

  const char* type = jsonDoc["type"];
  for(int i = 0; i < 4; i++) {
    title[i] = (char*)jsonDoc["applications"][i]["Title"];
    volume[i] = jsonDoc["applications"][i]["Volume"];
    color[i] = jsonDoc["applications"][i]["Color"];
  }

  render_full();
  

  pinMode(13, OUTPUT);
}

#define MINPRESSURE 10
#define MAXPRESSURE 1000

void loop()
{
  if(!menuOpen) {
    // Check for volume updates
    int newVolume[4];
    for (int i = 0; i < 4; i++) {
      newVolume[i] = map(analogRead(potPin[i]), 1024, 0, 0, 100);
    }
    for (int i = 0; i < 4; i++) {
      if (newVolume[i] != volume[i]) {
        volume[i] = newVolume[i];
        render_volume(i);
      }
    }
  }
  checkForSerialCommand();

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
      render_menu();
    } else {
      if(y < screen.Get_Display_Height()/3) {
        // Turn off display button pressed
        menuOpen = false;
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

void checkForSerialCommand(){
  static byte i = 0;
  char endMarker = '\n';
  char rc;
 
  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();
  
    if (rc != endMarker) {
      receivedChars[i] = rc;
      i++;
      if (i >= numChars) {
        i = numChars - 1;
      }
    }
    else {
      receivedChars[i] = '\0'; // terminate the string
      i = 0;

      newData = true;

      //Read from test json
      Serial.println(receivedChars);
      DeserializationError err = deserializeJson(jsonDoc, receivedChars);
      if(err) {
        Serial.print("ERROR:");
        Serial.println(err.c_str());
      }

      const char* type = jsonDoc["type"];
      if(strcmp(type, "data") != 0) {
        for(int i = 0; i < 4; i++) {
          char* newTitle = (char*)jsonDoc["applications"][i]["Title"];
          if(sizeof(newTitle) > 0) {
            title[i] = newTitle;
          }
          //volume[i] = jsonDoc["applications"][i]["Volume"];
          uint16_t newColor = jsonDoc["applications"][i]["Color"];
          if(newColor != BLACK) {
            color[i] = newColor;
          } 
        }
      }
      if(!err) {
        render_full();
      }
      
      newData = false;
    }
  }
}
