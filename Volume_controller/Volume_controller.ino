#include <TouchScreen.h> //touch library
#include <LCDWIKI_GUI.h> //Core graphics library
#include <LCDWIKI_KBV.h> //Hardware-specific library

#include <avr/pgmspace.h>
#include <ArduinoJson.h>

LCDWIKI_KBV screen(ILI9486, A3, A2, A1, A0, A4); //model,cs,cd,wr,rd,reset

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

// Arrays containing section specific data
// Should probably be in structs but this works
int16_t xOrigin[] = {0, SECTION_WIDTH, 0, SECTION_WIDTH};
int16_t yOrigin[] = {0, 0, SECTION_HEIGHT, SECTION_HEIGHT};
String title[] = {"", "", "", ""};
uint8_t volume[] = {100, 100, 100, 100};
uint16_t color[] = {BLACK, BLACK, BLACK, BLACK};
uint8_t potPin[] = {15, 14, 13, 12};
uint16_t icon[] = {NULL, NULL, NULL, NULL};
bool active[] = {false, false, false, false};
bool iconNeedsUpdate[] = {false, false, false, false};

String defaultDevice = "";
String devices[] = {"", "", "", "", "", ""};
bool deviceMenuOpen;

int logLevel = 5;


// Render just the volume bar for a section
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

// Request and render an icon
void render_icon(int section) {
  int16_t x = xOrigin[section] + ICON_PADDING;
  int16_t y = yOrigin[section] + ICON_PADDING;

  bool transEnded = false;

  // Request icon from server
  Serial.println("{\"type\": \"icon_request\", \"index\": " + String(section) + "}");

  // Loop to read data row by row and write individual pixels to display
  for (int yPos = 0; yPos < ICON_SIZE; yPos++) {
    while(!Serial.available()) {
      delay(10);
    }

    // Read row data
    byte row[ICON_SIZE * 2];
    Serial.readBytes(row, ICON_SIZE * 2);

    // Draw row
    for (int xPos = 0; xPos < ICON_SIZE; xPos++) {
      uint16_t color = word(row[xPos * 2], row[(xPos * 2) + 1]);

      // Draw pixel
      screen.Set_Draw_color(color);
      screen.Draw_Pixel(xPos + x, yPos + y);
    }
    
    // Send back acknowledgement
    serialFlush();
    Serial.write(0xFF);
  }

  while(!Serial.available()) {
    delay(10);
  }

  // Expect end byte (0xFF)
  uint16_t ack = 0;
  while(ack != 0xFF) {
    ack = Serial.read();
  }
  iconNeedsUpdate[section] = false;

  // Immediately check for serial update in case something changed while loading
  checkForSerialCommand();
}

// Render a full section (icon, title, volume)
void render_section(int section)
{
  // Render title
  render_text(section);

  // Render volume bar
  render_volume(section);
}

void clear_section(int section) {
  screen.Fill_Rect(xOrigin[section], yOrigin[section], SECTION_WIDTH, SECTION_HEIGHT, BLACK);
}

// Render all sections
void render_full(void)
{
  screen.Fill_Screen(BLACK);

  for (int i = 0; i < 4; i++) {
    if(active[i]) {
      render_section(i);
    }
  }
}

void render_device_menu(void) {
  int padding = 25;
  int buttonHeight = (screen.Get_Display_Height() - padding * 7) / 6;
  
  screen.Fill_Screen(BLACK);
  for(int i = 0; i < 6; i++) {
    int yPos = padding + (padding * i) + (buttonHeight * i);
    if(!devices[i].equals("") && !devices[i].equals(defaultDevice)) {
      // Render unselected device button
      screen.Set_Text_Mode(0);
      screen.Set_Text_colour(WHITE);
      screen.Set_Draw_color(WHITE);
      screen.Draw_Rectangle(
        padding, 
        yPos, 
        (screen.Get_Display_Width() - padding * 2) + padding, 
        buttonHeight + padding + (padding * i) + (buttonHeight * i)
      );
      screen.Print_String(
        devices[i].substring(0,21),
        padding + 10,
        yPos + 20
      );
    } else if (devices[i].equals(defaultDevice)) {
      // Render default device button
      screen.Set_Text_Mode(0);
      screen.Set_Text_colour(BLACK);
      screen.Fill_Rect(
        padding, 
        yPos,
        screen.Get_Display_Width() - padding * 2, 
        buttonHeight,
        WHITE
      );
      screen.Print_String(
        devices[i].substring(0,21),
        padding + 10,
        yPos + 20
      );
    }
  }
  return;
}

// Render touch menu
void render_menu(void)
{
  screen.Fill_Screen(BLACK);
  
  screen.Set_Text_Mode(1);
  screen.Set_Text_Size(3);
  screen.Set_Text_colour(WHITE);

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
    "Device",
    BUTTON_X_PADDING + 55, 
    BUTTON_Y_PADDING * 2 + BUTTON_HEIGHT + 25
  );
  screen.Print_String(
    "Chooser",
    BUTTON_X_PADDING + 45, 
    BUTTON_Y_PADDING * 2 + BUTTON_HEIGHT + 75
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

  reset_text_defaults();
}

void render_waiting_on_serial() {
  screen.Print_String("Waiting on", 0, 0);
  screen.Print_String("Serial connection", 0, 50);
}

void reset_text_defaults() {
  screen.Set_Text_Back_colour(BLACK);
  screen.Set_Text_Mode(0);
  screen.Set_Text_Size(2);
  screen.Set_Text_colour(WHITE);
}

void setup(void)
{
  Serial.begin(74880);
  screen.Init_LCD();
  screen.Set_Rotation(2);
  screen.Fill_Screen(BLACK);

  // Setup graphics engine defaults
  screen.Set_Draw_color(WHITE);
  reset_text_defaults();

  bool waiting = false;
  while(!Serial) {
    delay(500);
    if(!waiting && !Serial) {
      render_waiting_on_serial();
      waiting = true;
    }
  }
  screen.Fill_Screen(BLACK);

  checkForSerialCommand();  

  pinMode(13, OUTPUT);
}

#define MINPRESSURE 10
#define MAXPRESSURE 1000

void loop()
{
  if(!menuOpen && !deviceMenuOpen && !asleep) {    
    // Check for volume updates
    checkForVolumeChange();

    checkForSerialCommand();

    // Render 1 icon per program loop
    for(int i = 0; i < 4; i++){
      if(iconNeedsUpdate[i] && active[i]){
        render_icon(i);
        break;
      }
    }
  }

  // Do screen touching stuff
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE)
  {
    //Serial.println("[" + String(p.x) + "," + String(p.y) + "]");
    int x = map(p.x, 0, 1000, 0, screen.Get_Display_Width());
    int y = map(p.y, 0, 1000, 0, screen.Get_Display_Height());
    
    // Screen touched
    if (menuOpen) {
      if(y < screen.Get_Display_Height()/3) {
        // Turn off display button pressed
        menuOpen = false;
        asleep = true;
        screen.Fill_Screen(BLACK);
      } else if (y >= screen.Get_Display_Height()/3 && y < (screen.Get_Display_Height() * 2/3)) {
        // Button #2
        deviceMenuOpen = true;
        menuOpen = false;
        render_device_menu();
      } else if (y >= (screen.Get_Display_Height() * 2/3)) {
        // Close menu button pressed
        menuOpen = false;
        render_full();
      }
    } else if(deviceMenuOpen) {
      for(int i = 1; i <= 6; i++) {
        if(y <= screen.Get_Display_Height() * i/6 && !devices[i-1].equals("") && !devices[i-1].equals(defaultDevice)) {
          screen.Fill_Screen(BLACK);
          deviceMenuOpen = false;
          serialFlush();
          sendDeviceChangeRequest(i-1);
          while(!Serial.available()) {
            delay(10);
          }
          checkForSerialCommand();
          return;
        }
      }
    } else{
      menuOpen = true;
      asleep = false;
      render_menu();
      for(int i = 0; i < sizeof(iconNeedsUpdate); i++) {
        iconNeedsUpdate[i] = true;
      }
    }
  }
}

// Checks potentiometers for changes and requests updates
void checkForVolumeChange() {
  int newVolume[4];
  for (int i = 0; i < 4; i++) {
    newVolume[i] = map(analogRead(potPin[i]), 1024, 10, 0, 100);
    if(newVolume[i] > 100) {
      newVolume[i] = 100;
    }
  }
  for (int i = 0; i < 4; i++) {
    if (newVolume[i] != volume[i] && active[i]) {
      volume[i] = newVolume[i];
      render_volume(i);
      sendVolumeChangeRequest(i, volume[i]);
    }
  }
}

// Tell computer volume needs to change
void sendVolumeChangeRequest(int section, int volume) {
  Serial.println("{\"type\": \"volume_change\", \"index\": " + String(section) + ", \"volume\": " + String(volume) + "}");
}

void sendDeviceChangeRequest(int deviceIndex) {
  Serial.println("{\"type\": \"device_change\", \"deviceName\": \"" + devices[deviceIndex] + "\" }");
}

void sendLogRequest(String message, int level) {
  if(logLevel >= level) {
    Serial.println("{\"type\": \"log\", \"level\": " + String(level) + ", \"message\": \"" + message + "\" }");
  }
}

bool checkForSerialCommand(){

  // Check for and parse serial command
  if(Serial.available()){
    DynamicJsonBuffer jb;
    JsonObject& root = jb.parseObject(Serial);
    
    const char* type = root["type"];
    if(strcmp(type, "data") == 0) {
      // Loop through all apps and update any that have changed
      for(int i = 0; i < root["size"]; i++) {
        active[i] = true;

        //Weird string stuff I had to do to stop a memory reference bug causing undefined behavior
        const char* titleChar = (char*)root["applications"][i]["title"];
        String newTitle = titleChar;

        // If the title of the current section has changed redraw everything
        if(sizeof(newTitle) > 0 && newTitle.compareTo(title[i]) != 0) {
          sendLogRequest("Updating title: " + title[i] + " -> " + newTitle, 5);
          title[i] = newTitle;
          color[i] = root["applications"][i]["color"];
          iconNeedsUpdate[i] = true;
          clear_section(i);
          render_text(i);
          render_volume(i);
        }
      }
      // Set all apps not included in packet as inactive
      for(int i = 3; i >= root["size"]; i--)
      {
        active[i] = false;
        clear_section(i);
      }

      //Loop through and update audio device list
      const char* newDefaultDevice = (char*)root["defaultDevice"];
      defaultDevice = String(newDefaultDevice);
      for(int i = 0; i < root["deviceCount"]; i++ ) {
        const char* deviceChar = (char*)root["audioDevices"][i];
        String deviceName = deviceChar;
        devices[i] = deviceName;
      }
      for(int i = 5; i >= root["deviceCount"]; i--)
      {
        devices[i] = "";
      }
    }
  }
  
  return true;
}

// Wipe the incoming serial buffer
void serialFlush(){
  while(Serial.available() > 0) {
    char t = Serial.read();
  }
}   
