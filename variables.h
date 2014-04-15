/*************************/
/* Config Structure      */
/*************************/

struct config_t {
uint8_t vers;
uint8_t versEEPROM;

// Input settings
int16_t accTimeConstant;
int8_t  mpuLPF; // mpu LPF 0..6, 0=fastest(256Hz) 6=slowest(5Hz)
bool calibrateGyro;        // Else use EEPROM value
uint8_t majorAxis;
bool axisReverseZ;
bool axisSwapXY;

// PID settings
int32_t pitchKp; 
int32_t pitchKi;   
int32_t pitchKd;
int32_t rollKp;
int32_t rollKi;
int32_t rollKd;

// Output settings
int16_t angleOffsetPitch;   // angle offset, deg*100
int16_t angleOffsetRoll;
uint8_t nPolesMotorPitch;
uint8_t nPolesMotorRoll;
int8_t dirMotorPitch;
int8_t dirMotorRoll;
uint8_t motorNumberPitch;
uint8_t motorNumberRoll;
uint8_t maxPWMmotorPitch;
uint8_t maxPWMmotorRoll;

// RC settings
int8_t minRCPitch;
int8_t maxRCPitch;
int8_t minRCRoll;
int8_t maxRCRoll;
int16_t rcGain;
int16_t rcLPF;             // low pass filter for RC absolute mode, units=1/10 sec

// RC channels are not configurable. Makes no sense, when it is so easy 
// to just move the connections around instead.
// PPM mode is disabled for now (until I get a new interpreter written).
// Of course for PPM mode, channel settings will be needed.
// bool rcModePPM;          // RC mode, true=common RC PPM channel, false=separate RC channels 
// int8_t rcChannelPitch;     // input channel for pitch
// int8_t rcChannelRoll;      // input channel for roll
// int8_t rcChannelSwitch;    // input channel for switch

int16_t rcMid;             // rc channel center ms
bool rcAbsolute;
uint16_t crc16;
} config;

void recalcMotorStuff();
void initPIDs();

void setDefaultParameters() {
  config.vers = VERSION;
  config.versEEPROM = VERSION_EEPROM;
  config.pitchKp = 7500;
  config.pitchKi = 10000;
  config.pitchKd = 11000;
  config.rollKp = 20000;
  config.rollKi = 8000;
  config.rollKd = 30000;
  config.accTimeConstant = 4;
  config.mpuLPF = 0;
  config.angleOffsetPitch = 0;
  config.angleOffsetRoll = 0;
  config.nPolesMotorPitch = 14;
  config.nPolesMotorRoll = 14;
  config.dirMotorPitch = 1;
  config.dirMotorRoll = -1;
  config.motorNumberPitch = 0;
  config.motorNumberRoll = 1;
  config.maxPWMmotorPitch = 100;
  config.maxPWMmotorRoll = 100;
  config.minRCPitch = 0;
  config.maxRCPitch = 90;
  config.minRCRoll = -30;
  config.maxRCRoll = 30;
  config.rcGain = 5;
  config.rcLPF = 5;              // 0.5 sec
  config.rcMid = MID_RC;
  config.rcAbsolute = true;
  config.calibrateGyro = true;
  config.axisReverseZ=true;
  config.axisSwapXY=false;  
  config.majorAxis = 2;
  config.crc16 = 0;  
}

typedef struct PIDdata {
  int32_t   Kp, Ki, Kd;
} PIDdata_t;

PIDdata_t pitchPIDpar,rollPIDpar;

void initPIDs(void) {
  rollPIDpar.Kp = config.rollKp/10;
  rollPIDpar.Ki = config.rollKi/1000;
  rollPIDpar.Kd = config.rollKd/10;

  pitchPIDpar.Kp = config.pitchKp/10;
  pitchPIDpar.Ki = config.pitchKi/1000;
  pitchPIDpar.Kd = config.pitchKd/10;
}

// CRC definitions
#define POLYNOMIAL 0xD8  /* 11011 followed by 0's */
typedef uint8_t crc;

/*************************/
/* Variables             */
/*************************/

// motor drive
uint8_t pwmSinMotorPitch[256];
uint8_t pwmSinMotorRoll[256];

int currentStepMotor0 = 0;
int currentStepMotor1 = 0;
bool runMainLoop = false; 

int8_t pitchDirection = 1;
int8_t rollDirection = 1;

int freqCounter=0;

uint8_t motorPhases[2][3];
uint8_t softStart;

// Variables for MPU6050
float gyroPitch;
float gyroRoll; //in deg/s
float resolutionDivider;

int16_t x_val;
int16_t y_val;
int16_t z_val;

float PitchPhiSet;
float RollPhiSet;
static float pitchAngleSet;
static float rollAngleSet;

int count=0;

// RC control

struct rcData_t {
 uint16_t microsRisingEdge;
 uint16_t microsLastUpdate;
 uint16_t rx;
 bool     isFresh;
 bool     isValid;
 float    rcSpeed;
 float    setpoint;
};

rcData_t rcData[RC_DATA_SIZE];

enum extendSwitchType {
  RETRACTED_SW,
  MID_SW,
  EXTENDED_SW,
  UNKNOWN_SW
};
int8_t switchPos = UNKNOWN_SW;

float rcLPF_tc = 1.0;

// Gimbal State
enum gimStateType {
 GS_POWERUP=0,
 GS_RETRACTED,
 GS_EXTENDING,
 GS_EXTENDED
};

gimStateType gimState = GS_POWERUP;
int stateCount = 0;

//*************************************
//
//  IMU
//
//*************************************
struct flags_struct {
  uint8_t SMALL_ANGLES_25 : 1;
} flags;

//********************
// sensor orientation
//********************
enum axisDef {
  ROLL,
  PITCH,
  YAW
};

enum baseDef {
  X,
  Y,
  Z
};

typedef struct sensorAxisDef {
  char idx;
  int  dir;
} t_sensorAxisDef;

typedef struct sensorOrientationDef {
  t_sensorAxisDef Gyro[3];
  t_sensorAxisDef Acc[3];
} t_sensorOrientationDef;

t_sensorOrientationDef sensorDef;

// gyro calibration value
int16_t gyroOffset[3] = {0, 0, 0};

static float gyroScale=0;

static int32_t accSmooth[3];
static int16_t gyroADC[3];
static int16_t accADC[3];

static float EstG[3];
static float accLPF[3] = {0.0,};
static int32_t accMag = 0;

static float AccComplFilterConst = 0;  // filter constant for complementary filter

static int16_t acc_25deg = 25;      //** TODO: check
static int32_t angle[2]    = {0,0};  // absolute angle inclination in multiple of 0.01 degree    180 deg = 18000

// DEBUG only
uint32_t stackTop = 0xffffffff;
uint32_t stackBottom = 0;

uint32_t heapTop = 0;
uint32_t heapBottom = 0xffffffff;

uint8_t microMacro;

