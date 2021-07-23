#include "printables.h"

// Bitmasks
#define WIN_BITMASK             0b10000000 // Whether the player won or not
#define HELP_BITMASK            0b01110000 // To test if all help flags were set 
#define PAIR_BITMASK            0b01000000 // Whether the player requested the pair help
#define THREE_BITMASK           0b00100000 // Whether the player requested the prime help
#define PRIME_BITMASK           0b00010000 // Whether the player requested the multiple of three help
#define TRIES_BITMASK           0b00001100 // Number of tries counter (max 4 [0-3])
#define STATE_BITMASK           0b00000011 // States counter (4 states)
#define RESET                   0b00000000 // RESET state
#define INTERACT                0b00000001 // INTERACT state
#define PROMPT                  0b00000010 // PROMPT state
#define HALT                    0b00000011 // HALT state

// Constants
#define PRINT_TIME              20000UL  
#define RESPONSE_TIMEOUT        10000UL             

// Func prototypes
bool is_prime(uint8_t n);
bool read_ser();

// Globals
/*      
 FLAG'S BITS:
 ------------------------------------------------------------------------- 
 |  WIN  | PAIR  | THREE | PRIME |       TRIES       |       STATE       | 
 ------------------------------------------------------------------------- 
 | 1 bit | 1 bit | 1 bit | 1 bit | TRIES_1 | TRIES_0 | STATE_1 | STATE_0 | 
 ------------------------------------------------------------------------- 
*/
uint8_t flag;                  // All our flags in one flag
uint8_t number;                // The number the player will be guessing
unsigned long prev_time;

char c;                        // Vars for reading from serial 
char buf[5];
uint8_t idx = 0;

/************************************************************************/

void setup() {
  // Set serial and give it time to start
  Serial.begin(115200);
  delay(1000);
  
  // Seed the random number generator
  randomSeed(micros());

  // Print the HI message
  Serial.println(hi);
}

void loop() {
  switch(flag & STATE_BITMASK){
    
    case RESET:{
      flag = 0x00;
      // Set the number randomly
      number = random(0, 20);
      Serial.println("\n--- Your goal have been set, Good luck! ---");

      // Set the state to the interact state
      flag = (flag & ~STATE_BITMASK) | INTERACT;
      prev_time = millis();
      break;
    }// END RESET STATE

    case INTERACT:{

      if(millis() - prev_time >= PRINT_TIME){
        Serial.println("--- You there ? ---");
        prev_time = millis();
      }

      if((flag & (PAIR_BITMASK | PRIME_BITMASK | THREE_BITMASK)) == HELP_BITMASK){ // All help flags are set
        Serial.println("--- You've used all help available, which isn't permitted ---");
        flag = (flag & ~STATE_BITMASK) | PROMPT;
      }
      
      if(read_ser()){
        prev_time = millis();

        switch(buf[0]){
          
          case 'h':{ // Help was requested
            Serial.println(hi);
            break;
          }
          
          case 'n':{ // Received an answer
            char s[3];
            memcpy(s, buf+1, 2);
            uint8_t answer = strtol(s, NULL, 10);
            
            if(answer == number){ // Right answer
              flag |= WIN_BITMASK;
              flag = (flag & ~STATE_BITMASK) | PROMPT;
              break;
            }
            else{ // False answer
              uint8_t tries = (flag & TRIES_BITMASK) >> 2;
              Serial.printf("--- This is your %s attempt ! ---\n", count[tries]);
              if(tries == 2) flag = (flag & ~STATE_BITMASK) | PROMPT;
              else flag = (flag & ~TRIES_BITMASK) | ((tries + 1) << 2);
            }
            break;
          }
          
          case 'p':{  
            if(buf[1] == 'r'){
              Serial.printf("--- The number is%s prime ! ---\n", is_prime(number) ? "" : " not");
              flag |= PRIME_BITMASK;
            }
            else{
              Serial.printf("--- The number is%s pair ! ---\n", number % 2 == 0 ? "" : " not");
              flag |= PAIR_BITMASK;
            }
            break;
          }
          
          case 't':{
            Serial.printf("--- The number is%s a multiple of three ! ---\n", number % 3 == 0 ? "" : " not");
            flag |= THREE_BITMASK;
            break;
          }
          
          case 'e':{
            Serial.println("--- Game terminated ---");
            flag = (flag & ~STATE_BITMASK) | HALT;
            break;
          }
          
          default:{
            Serial.println("--- Unknown command ---");
            break;
          }
        }
        // Clear the buffer
        memset(buf, '0', 5);
      }
      break;
    }// END INTERACT STATE

    case PROMPT:{
      if(flag & WIN_BITMASK)Serial.println("--- YOU WON ! ---");
      else Serial.println("--- You lost ! ---");

      Serial.println("--- Wanna retry ? ---");
      Serial.println("Y/n ?");

      bool done = false;      
      unsigned long prev = millis();
      // Wait for the response with a timeout of 10s
      do{ 
        done = read_ser();
      }while(millis() - prev <= RESPONSE_TIMEOUT && !done);

      if(done){ // Got a response
        switch(buf[0]){
          case 'y':
          case 'Y':
            Serial.printf("--- Your number was %d, restarting the game ! ---\n", number);
            flag = (flag & ~STATE_BITMASK) | RESET;
            break;

          case 'n':
          case 'N':
            Serial.printf("--- Your number was %d, terminating the game ! ---\n", number);
            flag = (flag & ~STATE_BITMASK) | HALT;
            break;

          default: 
            Serial.println("--- Defaulted to NO ! ---");
            Serial.printf("--- Your number was %d, terminating the game ! ---\n", number);
            flag = (flag & ~STATE_BITMASK) | HALT;
            break;
        }
      }
      else{
        Serial.println("--- Defaulted to NO ! ---");
        Serial.printf("--- Your number was %d, terminating the game ! ---\n", number);
        flag = (flag & ~STATE_BITMASK) | HALT;
      }
      break;
    }// END PROMPT STATE

    case HALT:{
      unsigned long prev_t = millis();
      Serial.print("--- Following are the first 1024 digits of PI, enjoy ;) --- \nPI: ");
      uint16_t i = 0;
      while(i < 1024){
        if(millis() - prev_t >= 500){
          Serial.print(pi[i++]);
          prev_t = millis();
        }
      }
      break;
    }// END HALT STATE
    
    default: break;
  }// END STATE MACHINE
}

/************************************************************************/

// Returns whether there's something in the serial buffer or not (and reads it if there's something)
bool read_ser(){
  while(Serial.available() > 0){
    c = Serial.read();
    if(c != '\n' && idx < 5){ // Still reading
      buf[idx++] = c;
    }
    else{ // Done
      idx = 0;
      return true;
    }
  }
  return false;
}

// Returns whether the number is prime or not (not fast at all, good enough for a number <= than 20)
bool is_prime(uint8_t n){
  if(n <= 1) return false; // zero and one are not prime
  
  uint8_t i;
  for (i=2; i*i <= n; i++) {
      if (n % i == 0) return false;
  }
  return true;
}
