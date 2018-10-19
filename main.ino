////////////////////////////////////////////////////////////////////////////////////////////
//
// RFIDYun
// Arduino program uses RMD6300 to read RFID tokens
// Linino part stores RFID valid numbers in a file named users
// users format is similar to /etc/passwd: one line for each user
// line format is    RFIDid:username
// RFIDid is a 7 digit number


#include <FileIO.h>
#include <Bridge.h>


//DEBUG uses "serial" console. If console not available it freezes at the begining of the program
//#define DEBUG

//FILEDEBUG uses access log file to log button apen as well as rfid login
#define FILEDEBUG

//#include <SoftwareSerial.h>

#include <AltSoftSerial.h>
//AltSoftSerial work better than softwareserial (better performance)
//it uses a timer
 AltSoftSerial RFID;  //(RX=13, TX=5)

//SoftwareSerial RFID(2,8);  //(RX=2, TX=8)

unsigned char HexToNum(unsigned char digit);
int TagCheck(unsigned long int code, char *tag);
unsigned long int longCalcCode(char *tag);
int ReadUser(File&, char *tag, char *username);
String getTimeStamp();



//green led pin 8
//red led pin 10



#define GREEN 8
#define RED 10
#define BUTTON 11

void setup()
{
  RFID.begin(9600);    // start serial to RFID reader

  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);
  Bridge.begin();     //start communication with linino

#ifdef DEBUG
  Console.begin();    //start communication with ethernet console (300bps)
  while (!Console);   //wait until console is ready
  Console.println("Arduino starting......");
#endif

  /* //initialize comunication with the filesystem in the linino part
    FileSystem.begin();
    FileSystem.open("users",FILE_READ);
  */
}

void loop()
{

  char tag[14];
  int index = 0;
  int i;


  char rfid[10], user[25], userOK[25] = "";
  //create objets UsersFile, linked to users in linino, open for reading
  File UsersFile = FileSystem.open("/root/users", FILE_READ);

  if (RFID.available() > 13)  //RFID token is 13 bytes long, so I check if there is at least 13 bytes
  {
    do                        //making a sweep of the 13 bytes and putting all together in a string
    {
      i = RFID.read();

#ifdef DEBUG            //printing the code readed to the console terminal
      Console.print(i);
      Console.print(" ");
#endif

      tag[index++] = i;
    } while (i != 3);

#ifdef DEBUG
    Console.println(" ");  //sending LF to the console (if not it does not show)
#endif

    int OK = 0;
    while (ReadUser(UsersFile, rfid, user) == 0) //we will sweep all users file until user found or EOF
    {
      if (TagCheck(atol(rfid), tag))  //convert string to number and check against tag
      {
        OK = 1;
        strcpy(userOK, user);
      }
    }

    UsersFile.close();  //finished reading, file closed
 
    
    //creating log, it will add user to the end of the log file
    File LogFile = FileSystem.open("/root/accesslog", FILE_APPEND);

    if (OK)
    {
#ifdef DEBUG
      Console.println("OK");
      Console.print("Hello ");
      Console.println(userOK);
#endif


      LogFile.print(userOK);  //log access to file
      LogFile.print(" access at  "); LogFile.println(getTimeStamp());

      digitalWrite(GREEN, HIGH);
      delay(2000);
      digitalWrite(GREEN, LOW);

    }
    else
    {
#ifdef DEBUG
      Console.println("ERROR");
#endif

      digitalWrite(RED, HIGH);
      delay(2000);
      digitalWrite(RED, LOW);
    }

    LogFile.close();

    delay(3000);

    while (RFID.available())RFID.read(); //flush seem not to work well, so I read all data from RFID to empty it
    //without this, it reads 6 times each RFID.
    
    RFID.flush();


    

  }


  //getting date in seconds from midnight
  String sDate=getTimeStamp();
  String sDay=sDate.substring(0,1);
  String sHours=sDate.substring(11,13);
  String sMinutes=sDate.substring(14,16);
  String sSeconds=sDate.substring(17,19);
  unsigned long int seconds=3600*sHours.toInt()+60*sMinutes.toInt()+sSeconds.toInt();


  int OpenTheDoor=0;

  // calendar file /root/calendar
  //format HH:MM/O for open the door
  //and    HH:MM/C for close the door
  if (sDay.toInt()<6) //from monday to friday
  {
    File CalendarFile = FileSystem.open("/root/calendar", FILE_READ); 

    //Reading lines from the file. The lines are marked with the time in wich we open or close the door
    while(CalendarFile.available()>0)
    {
      String line=CalendarFile.readStringUntil('\n');
      if (line.length()==7 && line.substring(0,1)!="#")
      {
        //tranlating HH:MM to seconds from midnight
        unsigned long int LineSeconds=3600*(line.substring(0,2)).toInt()+60*(line.substring(3,5)).toInt();
        if (seconds>LineSeconds)
        {  
          if (line.substring(6,7)=="O") OpenTheDoor=1;
          if (line.substring(6,7)=="C") OpenTheDoor=0;
        }
      
      }
    }
    CalendarFile.close();
  }


  if(!digitalRead(BUTTON))  //user request to open the door  (pull-up)
  {
    if(OpenTheDoor) //if the schedule says the door should be opened now
    {
      #ifdef DEBUG
      Console.println(" / Open / "); 
      #endif 

      #ifdef FILEDEBUG
      File LogFile = FileSystem.open("/root/accesslog", FILE_APPEND);
      LogFile.print("BUTTON access at  "); LogFile.println(getTimeStamp());
      #endif

      
      digitalWrite(GREEN, HIGH);  //it will blink open 
      delay(1000);
      digitalWrite(GREEN, LOW);
    }
    else 
    {
      digitalWrite(GREEN, LOW);
      digitalWrite(RED,HIGH);
      delay(1000);
      digitalWrite(RED, LOW);
    }
  }

  
  

/*
  if(OpenTheDoor) //if the schedule says the door should be opened now
  {
    #ifdef DEBUG
    Console.println(" / Open / "); 
    #endif 
    digitalWrite(GREEN, HIGH);  //it will blink open 1s every 10s
    delay(1000);
    digitalWrite(GREEN, LOW);
    delay(9000);
  }
  else 
  {
    #ifdef DEBUG
    Console.println(" / close / ");
    #endif
    delay(10000);
  }
*/  
  

  
}




///////////////////////////////////////////////////////
// HexToNum
//
// Convert a HEX digit in ASCII format to a number with its value
//
unsigned char HexToNum(unsigned char digit)
{
  if ( (digit >= '0') && (digit <= '9') ) return digit - 48;
  if ( (digit >= 'A') && (digit <= 'F') ) return digit - 55;
  if ( (digit >= 'a') && (digit <= 'f') ) return digit - 87;
}



////////////////////////////////////////////////////////
//  TagCheck
//
//  Check decimal number printed on tag against tag readed
// by RFID sensor in HEX format
//
//   code -> decimal code printed on tag
//   tag[] -> character array containig tag in HEX format
//
int TagCheck(unsigned long int code, char *tag)
{
  unsigned char HexData[4];

  for (int i = 3, index = 0; i < 11; i = i + 2, index++) HexData[index] = (HexToNum(tag[i]) << 4) + HexToNum(tag[i + 1]);

  int i = 0;
  int checksum = 0x0A;
  unsigned long int CalcCode = HexData[0];

  while (i < 4)
  {
    checksum = checksum ^ HexData[i];
    CalcCode = (CalcCode << 8) + HexData[i++]; //generate the code using hex digits weights
  }

  return  code == CalcCode;

}


////////////////////////////////////////////////////////
//  longCalcCode
//
//  Convert RFID sensor in HEX format to numeric format
//  as printed in tags
//
//   code -> decimal code printed on tag
//   tag[] -> character array containig tag in HEX format
//
unsigned long int longCalcCode(char *tag)
{
  unsigned char HexData[4];

  for (int i = 3, index = 0; i < 11; i = i + 2, index++) HexData[index] = (HexToNum(tag[i]) << 4) + HexToNum(tag[i + 1]);

  int i = 0;
  int checksum = 0x0A;
  unsigned long int CalcCode = HexData[0];

  while (i < 4)
  {
    checksum = checksum ^ HexData[i];
    CalcCode = (CalcCode << 8) + HexData[i++]; //generate the code using hex digits weights
  }

  return CalcCode;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
// ReadUser
//
// Reads linino file for the next tag id
// Returns tagID in string format at tag and username also in string format at username
// Returns 0 if everything went OK and >0 if there is some error:
// 1  -> Error reading file /root/users
// 2 -> Error: /root/users may be empty?


int ReadUser(File& UsersFile, char *tag, char *username)
{
  if (UsersFile == 0) return 1;

  if (UsersFile.available() == 0) return 2; //no data found. Maybe EOF?

  char data[25] = "xx";
  int i = 0;


  while (UsersFile.available() > 0 && data[i] != '\n') //read file until EOF or LF found
  {
    data[i] = UsersFile.read();
    if (data[i] != '\n') i++; //next char until LF found
  }

  i = 0;
  do  //read tag, until : found
  {
    tag[i] = data[i];
  } while (data[++i] != ':');
  tag[i++] = '\0'; //add string terminator

  int n = 0;
  do  //read username, until LF found
  {
    username[n++] = data[i];
  } while (data[++i] != '\n');
  username[n] = '\0'; //add end of string

  return 0; //all went OK
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
// getTimeStamp()
//
// This function return a string with the time stamp
//
// From  https://www.arduino.cc/en/Tutorial/YunDatalogger
//
String getTimeStamp() 
{
  String result;
  Process time;
  // date is a command line utility to get the date and the time
  // in different formats depending on the additional parameter
  time.begin("date");
  time.addParameter("+%u %D %T");  // parameters: D for the complete date mm/dd/yy
  //             T for the time hh:mm:ss
  // u for week day number (1 is monday)
  time.run();  // run the command

  // read the output of the command
  while (time.available() > 0) {
    char c = time.read();
    if (c != '\n') {
      result += c;
    }
  }

  return result;
}
