/*This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.*/

#define THRESH_L 240
#define THRESH_H 260
#define PIVOT 240

#define CLR(x,y) (x&=(~(1<<y)))
#define SET(x,y) (x|=(1<<y))
#define XOR(x,y) (x^=(1<<y))

int goDel = 0;
int delimiter = 0;
byte command[22];
byte answer[160];
byte PC[16];
byte EPC[96];
byte RN16[16];
byte CRC[16];
unsigned int timing[320];
int i_glob, j_glob;
int pivot = PIVOT;

void timer1_setup (byte mode, int prescale, byte outmode_A, byte outmode_B, byte capture_mode);
void send_one();
void send_zero();
void send_preamble();
void send_RN16();
void send_EPC();
int check_crc5();
void compute_crc16();

void setup()
{
  int i;

  Serial.begin(9600);

  pinMode(13, OUTPUT); //EN
  pinMode(12, OUTPUT); //A1
  pinMode(11, OUTPUT); //A0
  pinMode(10, OUTPUT); //A0

  digitalWrite(13,LOW);
  digitalWrite(12,HIGH);
  digitalWrite(11,LOW);
  digitalWrite(10,HIGH);

  pinMode(8, INPUT);    
  digitalWrite(8, HIGH);
  for (i = 0; i < 16; i++)
      RN16[i] = 1;
  for (i = 0; i < 16; i++)
    PC[i] = 0;
  PC[2] = 1;
  PC[3] = 1;
  for (i = 0; i < 96; i++)
    EPC[i] = 1;
  compute_crc16();
  Serial.println("RFID Tag V0");
  Serial.print("EPC = ");
  for (i = 0; i<96; i++)
    Serial.print(EPC[i]);
  Serial.println();
  Serial.print("CRC = ");
  for (i = 0; i<16; i++)
    Serial.print(CRC[i]);
  Serial.println();
}

void loop()
{
  int i;
  int val;
  
  i_glob = 0;
  j_glob = 0;
  delimiter = 0;
  for (i = 0; i<150; i++)
    answer[i] = 0;
  for (i = 0; i<300; i++)
    timing[i] = 0;
  
  timer1_setup(0, 1, 0, 0, 0);

  rx_mode();
  //val = analogRead(0);

  if (goDel == 1) //Delimiter detected
  {
    //if (val > 50)
    {
    delayMicroseconds(600);
    bs_mode();
    if (answer[3] == 1 && answer[4] == 0 && answer[5] == 0 && answer[6] == 0) //Query command
    {
      send_RN16();
      delayMicroseconds(2000); // Should check ACK if we have time...
      //send_EPC();
      print_command();
      //Serial.println(val);
      //delay(1000);
      //while(1);
    }
  }
  goDel = 0;
  }
}

void send_0()
{
  XOR(PORTB, 4);
  XOR(PORTB, 3);
  delayMicroseconds(10);
  XOR(PORTB, 4);
  XOR(PORTB, 3);
  delayMicroseconds(10);  
}

void send_1()
{
  XOR(PORTB, 4);
  XOR(PORTB, 3);
  delayMicroseconds(20);
}


void send_preamble()
{
  send_1();
  send_0();
  send_1();
  send_0();
  delayMicroseconds(25); //Violation
  send_1();
}

void send_ext_preamble()
{
  int i;

  for (i = 0; i < 12; i++)
    send_0();
  send_1();
  send_0();
  send_1();
  send_0();
  delayMicroseconds(25); //Violation
  send_1();
}


void send_RN16()
{
  int i;

  send_preamble();
  for (i = 0; i < 16; i++)
  {
    if(RN16[i] == 0)
      send_0();
    else
      send_1();
  }
  send_1();      //Dummy
}

void send_EPC()
{
  int i;

  send_preamble();
  for (i = 0; i < 16; i++) //Send PC
  {
    if (PC[i] == 0)
      send_0();
    else
      send_1();
  }
  for (i = 0; i < 96; i++)
  {
    if (EPC[i] == 0)
      send_0();
    else
      send_1();
  }
  for (i = 0; i < 16; i++)
  {
    if (CRC[i] == 0)
      send_0();
    else
      send_1();
  }
  send_1();      //Dummy (not sure)

}

int check_crc5()
{
  int i, j;
  byte reg[5];
  byte gen[6] = {1, 0, 1, 0, 0, 1};
  byte res[5] = {0, 0, 0, 0, 0};
  byte pre[5] = {0, 1, 0, 0, 1};
  
  byte trame[27];
  int error = 0;

  for (i = 0; i < 27; i++)
    trame[i] = 0;
  for (i = 0; i < 22; i++)
    trame[i] = answer[i+3];

  for (i = 0; i < 5; i++)
  {
    if (pre[i])
      reg[i] = trame[i] ? 0:1;  //preset
    else
      reg[i] = trame[i];
  }

  for (i = 5; i < 22-4; i++)
  {
    if (reg[0] == 1)
    {
      for (j= 0; j < 4; j++)
        reg[j] = (reg[j+1] + gen[j+1])%2;
      reg[4] = (trame[i]+ gen[5])%2;
    }
    else
    {
      for (j = 0; j < 4; j++)
        reg[j] = reg[j+1];
      reg[4] = trame[i];
    }
  }
  for (i = 0; i < 5; i++)
  {
    if (reg[i] != res[i])
    {
      error = 1;
      break;
    }
  }
  return error;
}

void compute_crc16()
{
  int i, j;
  byte reg[16];
  byte gen[17] = {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  
  byte trame[128];
  
  for (i = 0; i < 128; i++)
    trame[i] = 0;
  for (i = 0; i < 16; i++)
    trame[i] = PC[i];
  for (i = 0; i < 96; i++)
    trame[i+16] = EPC[i];
  for (i = 0; i < 16; i++)
    trame[i+112] = 0;

  for (i = 0; i < 16; i++)
    reg[i] = trame[i] ? 0:1;  //preset

  for (i = 16; i < 128; i++)
  {
    if (reg[0] == 1)
    {
      for (j= 0; j < 15; j++)
        reg[j] = (reg[j+1] + gen[j+1])%2;
      reg[15] = (trame[i]+ gen[16])%2;
    }
    else
    {
      for (j = 0; j < 15; j++)
        reg[j] = reg[j+1];
      reg[15] = trame[i];
    }
  }
  for (i = 0; i < 16; i++)
  {
    CRC[i] = reg[i] ? 0:1;
  }
}



void timer1_setup (byte mode, int prescale, byte outmode_A, byte outmode_B, byte capture_mode)
{
  // enforce field widths for sanity
  mode &= 15 ;
  outmode_A &= 3 ;
  outmode_B &= 3 ;
  capture_mode &= 3 ;

  byte clock_mode = 0 ; // 0 means no clocking - the counter is frozen.
  switch (prescale)
  {
    case 1: clock_mode = 1 ; break ;
    case 8: clock_mode = 2 ; break ;
    case 64: clock_mode = 3 ; break ;
    case 256: clock_mode = 4 ; break ;
    case 1024: clock_mode = 5 ; break ;
    default:
      if (prescale < 0)
        clock_mode = 7 ; // external clock
  }
  TCNT1 = 0;
  TCCR1A = (outmode_A << 6) | (outmode_B << 4) | (mode & 3) ;
  TCCR1B = (capture_mode << 6) | ((mode & 0xC) << 1) | clock_mode ;
}

void read_answer(int timeout)
{
  TIFR1 = 1<<5;
  TIMSK1 = 1<<5;
  TCNT1 = 0;
  delayMicroseconds(timeout);
  TIFR1 = 0<<5;
  TIMSK1 = 0<<5;
}

void rx_mode()
{
  TIFR1 = 1<<5;
  TIMSK1 = 1<<5;
  TCNT1 = 0;
}

void bs_mode()
{
  TIFR1 = 0<<5;
  TIMSK1 = 0<<5;
}

void print_command()
{
  int i;
  int Del;
  int RTCAL;
  int TRCAL;
  int pivottemp;

  Del = timing[delimiter]-timing[delimiter-1];
  Serial.print("Del = ");
  Serial.print(Del/16.);
  RTCAL = timing[delimiter+4]-timing[delimiter+2];
  Serial.print("; RTCAL = ");
  Serial.print(RTCAL/16.);
  TRCAL = timing[delimiter+6]-timing[delimiter+4];
  Serial.print("; TRCAL = ");
  Serial.print(TRCAL/16.);
  //pivottemp = RTCAL/2 - (timing[delimiter+2]-timing[delimiter+1]);//pivot = RTCAL/2; //Tricky
  //if (pivottemp > 0)
  //  pivot = pivottemp;
  Serial.print("; PIVOT = ");
  Serial.println(pivot/16.);
  if (check_crc5()==0)
  {
    Serial.println("Query detected");
    for (i = 0; i < 22; i++)
      Serial.print(answer[i+3]);
    Serial.println(); 
    if (answer[7] == 0)
      Serial.print("DR = 8");
    else
      Serial.print("DR = 63/3");
    Serial.print("; M = ");
    Serial.print(2*answer[8]+answer[9]);
    Serial.print("; TRext = ");
    Serial.print(answer[10]);
    Serial.print("; Sel = ");
    Serial.print(2*answer[11]+answer[12]);
    Serial.print("; Session = ");
    Serial.print(2*answer[13]+answer[14]);
    Serial.print("; Target = ");
    if (answer[15] == 0)
      Serial.print("A");
    else
      Serial.print("B");
    Serial.print("; Q = ");
    Serial.println(8*answer[16]+4*answer[17]+2*answer[18]+answer[19]);
    Serial.println("CRC 0K");
  }
  else
    Serial.println("CRC fail");

  for (i = 0; i<80; i++)
  {
    Serial.print(" ");
    Serial.print(timing[i]);
  }
  Serial.println();
  for (i = 1; i<80; i++)
  {
    Serial.print(" ");
    Serial.print(timing[i]-timing[i-1]);
  }
  Serial.println();
  for (i = 0; i<80; i++)
  {
    Serial.print(" ");
    Serial.print(answer[i]);
  }
  Serial.println();
}

ISR(TIMER1_CAPT_vect)
{
  static unsigned int time = 0, state = 0;
  
  TCCR1B = TCCR1B ^ (1<<6);
  timing[i_glob] = ICR1;
  time = timing[i_glob] - timing[i_glob-1];
  if (time > THRESH_L && time < THRESH_H  && delimiter == 0) //Delimiter
  {
    goDel = 1;
    delimiter = i_glob;
  }
  i_glob = i_glob + 1;
  if (delimiter != 0)
  {
    if ((state%2)==1)
    {
      if (time < pivot)
        answer[j_glob++] = 0;
      if (time > pivot)
        answer[j_glob++] = 1;
    }
    state++;
  }
}
