/*
    This file is part of Repetier-Firmware.

    Repetier-Firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Repetier-Firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Repetier-Firmware.  If not, see <http://www.gnu.org/licenses/>.

    This firmware is a nearly complete rewrite of the sprinter firmware
    by kliment (https://github.com/kliment/Sprinter)
    which based on Tonokip RepRap firmware rewrite based off of Hydra-mmm firmware.

    Main author: repetier

*/
/**
\mainpage Repetier-Firmware for Arduino based RepRaps
<CENTER>Copyright &copy; 2011-2013 by repetier
</CENTER>

\section Intro Introduction


\section GCodes Implemented GCodes

 look here for descriptions of gcodes: http://linuxcnc.org/handbook/gcode/g-code.html
 and http://objects.reprap.org/wiki/Mendel_User_Manual:_RepRapGCodes

Implemented Codes

- G0  -> G1
- G1  - Coordinated Movement X Y Z E
- G4  - Dwell S<seconds> or P<milliseconds>
- G20 - Units for G0/G1 are inches.
- G21 - Units for G0/G1 are mm.
- G28 - Home all axis or named axis.
- G90 - Use absolute coordinates
- G91 - Use relative coordinates
- G92 - Set current position to cordinates given

RepRap M Codes

- M104 - Set extruder target temp
- M105 - Read current temp
- M106 - Fan on
- M107 - Fan off
- M109 - Wait for extruder current temp to reach target temp.
- M114 - Display current position

Custom M Codes

- M80  - Turn on Power Supply
- M20  - List SD card
- M21  - Init SD card
- M22  - Release SD card
- M23  - Select SD file (M23 filename.g)
- M24  - Start/resume SD print
- M25  - Pause SD print
- M26  - Set SD position in bytes (M26 S12345)
- M27  - Report SD print status
- M28  - Start SD write (M28 filename.g)
- M29  - Stop SD write
- M30 <filename> - Delete file on sd card
- M32 <dirname> create subdirectory
- M42 P<pin number> S<value 0..255> - Change output of pin P to S. Does not work on most important pins.
- M80  - Turn on power supply
- M81  - Turn off power supply
- M82  - Set E codes absolute (default)
- M83  - Set E codes relative while in Absolute Coordinates (G90) mode
- M84  - Disable steppers until next move,
        or use S<seconds> to specify an inactivity timeout, after which the steppers will be disabled.  S0 to disable the timeout.
- M85  - Set inactivity shutdown timer with parameter S<seconds>. To disable set zero (default)
- M92  - Set axis_steps_per_unit - same syntax as G92
- M112 - Emergency kill
- M115- Capabilities string
- M117 <message> - Write message in status row on lcd
- M119 - Report endstop status
- M140 - Set bed target temp
- M190 - Wait for bed current temp to reach target temp.
- M201 - Set max acceleration in units/s^2 for print moves (M201 X1000 Y1000)
- M202 - Set max acceleration in units/s^2 for travel moves (M202 X1000 Y1000)
- M203 - Set temperture monitor to Sx
- M204 - Set PID parameter X => Kp Y => Ki Z => Kd S<extruder> Default is current extruder. NUM_EXTRUDER=Heated bed
- M205 - Output EEPROM settings
- M206 - Set EEPROM value
- M220 S<Feedrate multiplier in percent> - Increase/decrease given feedrate
- M221 S<Extrusion flow multiplier in percent> - Increase/decrease given flow rate
- M231 S<OPS_MODE> X<Min_Distance> Y<Retract> Z<Backlash> F<ReatrctMove> - Set OPS parameter
- M232 - Read and reset max. advance values
- M233 X<AdvanceK> Y<AdvanceL> - Set temporary advance K-value to X and linear term advanceL to Y
- M251 Measure Z steps from homing stop (Delta printers). S0 - Reset, S1 - Print, S2 - Store to Z length (also EEPROM if enabled)
- M303 P<extruder/bed> S<drucktermeratur> Autodetect pid values. Use P<NUM_EXTRUDER> for heated bed.
- M350 S<mstepsAll> X<mstepsX> Y<mstepsY> Z<mstepsZ> E<mstepsE0> P<mstespE1> : Set microstepping on RAMBO board
- M400 - Wait until move buffers empty.
- M401 - Store x, y and z position.
- M402 - Go to stored position. If X, Y or Z is specified, only these coordinates are used. F changes feedrate fo rthat move.
- M500 Store settings to EEPROM
- M501 Load settings from EEPROM
- M502 Reset settings to the one in configuration.h. Does not store values in EEPROM!
- M908 P<address> S<value> : Set stepper current for digipot (RAMBO board)
*/

#include "Repetier.h"
#include "Eeprom.h"
#include "pins_arduino.h"
#include "fastio.h"
#include "ui.h"
#include <util/delay.h>
#include <SPI.h>

#if UI_DISPLAY_TYPE==4
//#include <LiquidCrystal.h> // Uncomment this if you are using liquid crystal library
#endif

// ================ Sanity checks ================
#ifndef STEP_DOUBLER_FREQUENCY
#error Please add new parameter STEP_DOUBLER_FREQUENCY to your configuration.
#else
#if STEP_DOUBLER_FREQUENCY<10000 || STEP_DOUBLER_FREQUENCY>20000
#error STEP_DOUBLER_FREQUENCY should be in range 10000-16000.
#endif
#endif
#ifdef EXTRUDER_SPEED
#error EXTRUDER_SPEED is not used any more. Values are now taken from extruder definition.
#endif
#if MAX_HALFSTEP_INTERVAL<=1900
#error MAX_HALFSTEP_INTERVAL must be greater then 1900
#endif
#ifdef ENDSTOPPULLUPS
#error ENDSTOPPULLUPS is now replaced by individual pullup configuration!
#endif
#ifdef EXT0_PID_PGAIN
#error The PID system has changed. Please use the new float number options!
#endif
// ####################################################################################
// #          No configuration below this line - just some errorchecking              #
// ####################################################################################
#ifdef SUPPORT_MAX6675
#if !defined SCK_PIN || !defined MOSI_PIN || !defined MISO_PIN
#error For MAX6675 support, you need to define SCK_PIN, MISO_PIN and MOSI_PIN in pins.h
#endif
#endif
#if X_STEP_PIN<0 || Y_STEP_PIN<0 || Z_STEP_PIN<0
#error One of the following pins is not assigned: X_STEP_PIN,Y_STEP_PIN,Z_STEP_PIN
#endif
#if EXT0_STEP_PIN<0 && NUM_EXTRUDER>0
#error EXT0_STEP_PIN not set to a pin number.
#endif
#if EXT0_DIR_PIN<0 && NUM_EXTRUDER>0
#error EXT0_DIR_PIN not set to a pin number.
#endif
#if MOVE_CACHE_SIZE<4
#error MOVE_CACHE_SIZE must be at least 5
#endif

#if DRIVE_SYSTEM==3
#define SIN_60 0.8660254037844386
#define COS_60 0.5
#define DELTA_DIAGONAL_ROD_STEPS (AXIS_STEPS_PER_MM * DELTA_DIAGONAL_ROD)
#define DELTA_DIAGONAL_ROD_STEPS_SQUARED (DELTA_DIAGONAL_ROD_STEPS * DELTA_DIAGONAL_ROD_STEPS)
#define DELTA_ZERO_OFFSET_STEPS (AXIS_STEPS_PER_MM * DELTA_ZERO_OFFSET)
#define DELTA_RADIUS_STEPS (AXIS_STEPS_PER_MM * DELTA_RADIUS)

#define DELTA_TOWER1_X_STEPS -SIN_60*DELTA_RADIUS_STEPS
#define DELTA_TOWER1_Y_STEPS -COS_60*DELTA_RADIUS_STEPS
#define DELTA_TOWER2_X_STEPS SIN_60*DELTA_RADIUS_STEPS
#define DELTA_TOWER2_Y_STEPS -COS_60*DELTA_RADIUS_STEPS
#define DELTA_TOWER3_X_STEPS 0.0
#define DELTA_TOWER3_Y_STEPS DELTA_RADIUS_STEPS

#define NUM_AXIS 4
#define X_AXIS 0
#define Y_AXIS 1
#define Z_AXIS 2
#define E_AXIS 3

#endif

#define OVERFLOW_PERIODICAL  (int)(F_CPU/(TIMER0_PRESCALE*40))
// RAM usage of variables: Non RAMPS 114+MOVE_CACHE_SIZE*59+printer_state(32) = 382 Byte with MOVE_CACHE_SIZE=4
// RAM usage RAMPS adds: 96
// RAM usage SD Card:

//Inactivity shutdown variables
unsigned long previous_millis_cmd = 0;
unsigned long max_inactive_time = MAX_INACTIVE_TIME*1000L;
unsigned long stepper_inactive_time = STEPPER_INACTIVE_TIME*1000L;
long baudrate = BAUDRATE;         ///< Communication speed rate.
#ifdef USE_ADVANCE
#ifdef ENABLE_QUADRATIC_ADVANCE
int maxadv=0;
#endif
int maxadv2=0;
float maxadvspeed=0;
#endif
byte pwm_pos[NUM_EXTRUDER+3]; // 0-NUM_EXTRUDER = Heater 0-NUM_EXTRUDER of extruder, NUM_EXTRUDER = Heated bed, NUM_EXTRUDER+1 Board fan, NUM_EXTRUDER+2 = Fan

int waitRelax=0; // Delay filament relax at the end of print, could be a simple timeout
#ifdef DEBUG_FREE_MEMORY
int lowest_ram=16384;
int lowest_send=16384;

void check_mem()
{
    BEGIN_INTERRUPT_PROTECTED
    uint8_t * heapptr, * stackptr;
    heapptr = (uint8_t *)malloc(4);          // get heap pointer
    free(heapptr);      // free up the memory again (sets heapptr to 0)
    stackptr =  (uint8_t *)(SP);           // save value of stack pointer
    int newfree = (int)stackptr-(int)heapptr;
    if(newfree<lowest_ram)
    {
        lowest_ram = newfree;
    }
    END_INTERRUPT_PROTECTED
}
void send_mem()
{
    if(lowest_send>lowest_ram)
    {
        lowest_send = lowest_ram;
        out.println_int_P(PSTR("Free RAM:"),lowest_ram);
    }
}
#endif




/** \brief Setup of the hardware

Sets the output and input pins in accordance to your configuration. Initializes the serial interface.
Interrupt routines to measure analog values and for the stepper timerloop are started.
*/
void setup()
{
    Printer::setup();
}

/**
  Main processing loop. It checks perodically for new commands, checks temperatures
  and executes new incoming commands.
*/
void loop()
{
    GCode::readFromSerial();
    GCode *code = GCode::nextCommand();
    //UI_SLOW; // do longer timed user interface action
    UI_MEDIUM; // do check encoder
    if(code)
    {
#if SDSUPPORT
        if(sd.savetosd)
        {
            if(!(code->hasM() && code->M==29))   // still writing to file
            {
                sd.write_command(code);
            }
            else
            {
                sd.finishWrite();
            }
#ifdef ECHO_ON_EXECUTE
            if(Printer::debugEcho())
            {
                OUT_P("Echo:");
                code->printCommand();
                out.println();
            }
#endif
            code->commandFinished();
        }
        else
        {
            Commands::executeGCode(code,true);
        }
#else
        Commands::executeGCode(code,true);
#endif
    }
    Printer::defaultLoopActions();
}





/**
  Moves the stepper motors one step. If the last step is reached, the next movement is started.
  The function must be called from a timer loop. It returns the time for the next call.
  This is a modified version that implements a bresenham 'multi-step' algorithm where the dominant
  cartesian axis steps may be less than the changing dominant delta axis.
*/
#if DRIVE_SYSTEM==3
int lastblk=-1;
long cur_errupd;
//#define DEBUG_DELTA_TIMER
// Current delta segment
DeltaSegment *curd;
// Current delta segment primary error increment
long curd_errupd, stepsPerSegRemaining;
long PrintLine::bresenhamStep()
{
    if(PrintLine::cur == 0)
    {
        HAL::allowInterrupts();
        PrintLine::setCurrentLine();
        if(PrintLine::cur->isBlocked())   // This step is in computation - shouldn't happen
        {
            if(lastblk!=(int)cur)
            {
                lastblk = (int)cur;
                out.println_int_P(PSTR("BLK "),(unsigned int)lines_count);
            }
            cur = 0;
            return 2000;
        }
        lastblk = -1;
#ifdef INCLUDE_DEBUG_NO_MOVE
        if(DEBUG_NO_MOVES)   // simulate a move, but do nothing in reality
        {
            PrintLine::popLineForbidInterrupt();
            return 1000;
        }
#endif
        if(PrintLine::cur->isWarmUp())
        {
            // This is a warmup move to initalize the path planner correctly. Just waste
            // a bit of time to get the planning up to date.
            if(lines_count<=cur->primaryAxis)
            {
                cur=0;
                return 2000;
            }
            long wait = PrintLine::PrintLine::cur->getWaitTicks();
            PrintLine::popLineForbidInterrupt();
            return(wait); // waste some time for path optimization to fill up
        } // End if WARMUP
        if(PrintLine::PrintLine::cur->isEMove()) Extruder::enable();
        PrintLine::cur->fixStartAndEndSpeed();
        HAL::allowInterrupts(); // Allow interrupts
        // Set up delta segments
        if (PrintLine::cur->numDeltaSegments)
        {
            // If there are delta segments point to them here
            curd = &segments[PrintLine::cur->deltaSegmentReadPos++];
            if (PrintLine::cur->deltaSegmentReadPos >= DELTA_CACHE_SIZE) PrintLine::cur->deltaSegmentReadPos=0;
            // Enable axis - All axis are enabled since they will most probably all be involved in a move
            // Since segments could involve different axis this reduces load when switching segments and
            // makes disabling easier.
            Printer::enableXStepper();
            Printer::enableYStepper();
            Printer::enableZStepper();

            // Copy across movement into main direction flags so that endstops function correctly
            PrintLine::cur->dir |= curd->dir;
            // Initialize bresenham for the first segment
            if (PrintLine::cur->halfstep)
            {
                PrintLine::cur->error[0] = PrintLine::cur->error[1] = PrintLine::cur->error[2] = PrintLine::cur->numPrimaryStepPerSegment;
                curd_errupd = PrintLine::cur->numPrimaryStepPerSegment = PrintLine::cur->numPrimaryStepPerSegment<<1;
            }
            else
            {
                PrintLine::cur->error[0] = PrintLine::cur->error[1] = PrintLine::cur->error[2] = PrintLine::cur->numPrimaryStepPerSegment>>1;
                curd_errupd = PrintLine::cur->numPrimaryStepPerSegment;
            }
            stepsPerSegRemaining = PrintLine::cur->numPrimaryStepPerSegment;
#ifdef DEBUG_DELTA_TIMER
            out.println_byte_P(PSTR("HS: "),PrintLine::cur->halfstep);
            out.println_long_P(PSTR("Error: "),curd_errupd);
#endif
        }
        else curd=0;
        cur_errupd = (PrintLine::cur->halfstep ? PrintLine::cur->stepsRemaining << 1 : PrintLine::cur->stepsRemaining);

        if(!(PrintLine::cur->joinFlags & FLAG_JOIN_STEPPARAMS_COMPUTED))  // should never happen, but with bad timings???
        {
            out.println_int_P(PSTR("LATE "),(unsigned int)lines_count);
            cur->updateStepsParameter();
        }
        printer.vMaxReached = PrintLine::cur->vStart;
        printer.stepNumber=0;
        printer.timer = 0;
        HAL::forbidInterrupts();
        //Determine direction of movement
        if (curd)
        {
            if(curd->dir & 1)
            {
                WRITE(X_DIR_PIN,!INVERT_X_DIR);
            }
            else
            {
                WRITE(X_DIR_PIN,INVERT_X_DIR);
            }
            if(curd->dir & 2)
            {
                WRITE(Y_DIR_PIN,!INVERT_Y_DIR);
            }
            else
            {
                WRITE(Y_DIR_PIN,INVERT_Y_DIR);
            }
            if(curd->dir & 4)
            {
                WRITE(Z_DIR_PIN,!INVERT_Z_DIR);
            }
            else
            {
                WRITE(Z_DIR_PIN,INVERT_Z_DIR);
            }
        }
#if defined(USE_ADVANCE)
        if(!printer.isAdvanceActivated()) // Set direction if no advance/OPS enabled
#endif
            if(PrintLine::cur->isEPositiveMove())
            {
                Extruder::setDirection(1);
            }
            else
            {
                Extruder::setDirection(0);
            }
#ifdef USE_ADVANCE
        long h = HAL::mulu16xu16to32(PrintLine::cur->vStart,PrintLine::cur->advanceL);
        int tred = ((
#ifdef ENABLE_QUADRATIC_ADVANCE
                        (printer.advance_executed = PrintLine::cur->advanceStart)+
#endif
                        h)>>16);
        printer.extruderStepsNeeded+=tred-printer.advance_steps_set;
        printer.advance_steps_set = tred;
#endif
        if(printer.waslasthalfstepping && PrintLine::cur->halfstep==0)   // Switch halfstepping -> full stepping
        {
            printer.waslasthalfstepping = 0;
            return Printer::interval*3; // Wait an other 150% from last half step to make the 100% full
        }
        else if(!printer.waslasthalfstepping && PrintLine::cur->halfstep)     // Switch full to half stepping
        {
            printer.waslasthalfstepping = 1;
        }
        else
            return Printer::interval; // Wait an other 50% from last step to make the 100% full
    } // End cur=0
    HAL::allowInterrupts();

    /* For halfstepping, we divide the actions into even and odd actions to split
       time used per loop. */
    byte do_even;
    byte do_odd;
    if(PrintLine::cur->halfstep)
    {
        do_odd = PrintLine::cur->halfstep & 1;
        do_even = PrintLine::cur->halfstep & 2;
        PrintLine::cur->halfstep = 3-PrintLine::cur->halfstep;
    }
    else
    {
        do_even = 1;
        do_odd = 1;
    }
    HAL::forbidInterrupts();
    if(do_even)
    {
        if((PrintLine::cur->flags & FLAG_CHECK_ENDSTOPS) && (curd != 0))
        {
#if X_MAX_PIN>-1 && MAX_HARDWARE_ENDSTOP_X
            if((curd->dir & 17)==17) if(READ(X_MAX_PIN) != ENDSTOP_X_MAX_INVERTING)
                {
                    curd->dir&=~16;
                    PrintLine::cur->dir&=~16;
                }
#endif
#if Y_MAX_PIN>-1 && MAX_HARDWARE_ENDSTOP_Y
            if((curd->dir & 34)==34) if(READ(Y_MAX_PIN) != ENDSTOP_Y_MAX_INVERTING)
                {
                    curd->dir&=~32;
                    PrintLine::cur->dir&=~32;
                }
#endif
#if Z_MAX_PIN>-1 && MAX_HARDWARE_ENDSTOP_Z
            if((curd->dir & 68)==68) if(READ(Z_MAX_PIN)!= ENDSTOP_Z_MAX_INVERTING)
                {
                    curd->dir&=~64;
                    PrintLine::cur->dir&=~64;
                }
#endif
        }
    }
    byte max_loops = (printer.stepper_loops<=PrintLine::cur->stepsRemaining ? printer.stepper_loops : PrintLine::cur->stepsRemaining);
    if(PrintLine::cur->stepsRemaining>0)
    {
        for(byte loop=0; loop<max_loops; loop++)
        {
            if(loop>0)
#if STEPPER_HIGH_DELAY>0
                HAL::delayMicroseconds(STEPPER_HIGH_DELAY+DOUBLE_STEP_DELAY);
#else
                HAL::delayMicroseconds(DOUBLE_STEP_DELAY);
#endif
            if(PrintLine::cur->dir & 128)
            {
                if((PrintLine::cur->error[3] -= PrintLine::cur->delta[3]) < 0)
                {
#if defined(USE_ADVANCE)
                    if((printer.flag0 & PRINTER_FLAG0_SEPERATE_EXTRUDER_INT))   // Use interrupt for movement
                    {
                        if(PrintLine::cur->dir & 8)
                            printer.extruderStepsNeeded++;
                        else
                            printer.extruderStepsNeeded--;
                    }
                    else
                    {
#endif
                        Extruder::step();
#if defined(USE_ADVANCE)
                    }
#endif
                    PrintLine::cur->error[3] += cur_errupd;
                }
            }
            if (curd)
            {
                // Take delta steps
                if(curd->dir & 16)
                {
                    if((PrintLine::cur->error[0] -= curd->deltaSteps[0]) < 0)
                    {
                        cur->startXStep();
                        PrintLine::cur->error[0] += curd_errupd;
                    }
                }

                if(curd->dir & 32)
                {
                    if((PrintLine::cur->error[1] -= curd->deltaSteps[1]) < 0)
                    {
                        cur->startYStep();
                        PrintLine::cur->error[1] += curd_errupd;
                    }
                }

                if(curd->dir & 64)
                {
                    if((PrintLine::cur->error[2] -= curd->deltaSteps[2]) < 0)
                    {
                        WRITE(Z_STEP_PIN,HIGH);
                        printer.countZSteps += ( PrintLine::cur->dir & 4 ? 1 : -1 );
                        PrintLine::cur->error[2] += curd_errupd;
#ifdef DEBUG_STEPCOUNT
                        PrintLine::cur->totalStepsRemaining--;
#endif
                    }
                }

#if STEPPER_HIGH_DELAY>0
                HAL::delayMicroseconds(STEPPER_HIGH_DELAY);
#endif
                Printer::endXYZSteps();
                stepsPerSegRemaining--;
                if (!stepsPerSegRemaining)
                {
                    PrintLine::cur->numDeltaSegments--;
                    if (PrintLine::cur->numDeltaSegments)
                    {

                        // Get the next delta segment
                        curd = &segments[PrintLine::cur->deltaSegmentReadPos++];
                        if (PrintLine::cur->deltaSegmentReadPos >= DELTA_CACHE_SIZE) PrintLine::cur->deltaSegmentReadPos=0;
                        delta_segment_count--;

                        // Initialize bresenham for this segment (numPrimaryStepPerSegment is already correct for the half step setting)
                        PrintLine::cur->error[0] = PrintLine::cur->error[1] = PrintLine::cur->error[2] = PrintLine::cur->numPrimaryStepPerSegment>>1;

                        // Reset the counter of the primary steps. This is initialized in the line
                        // generation so don't have to do this the first time.
                        stepsPerSegRemaining = PrintLine::cur->numPrimaryStepPerSegment;

                        // Change direction if necessary
                        if(curd->dir & 1)
                        {
                            WRITE(X_DIR_PIN,!INVERT_X_DIR);
                        }
                        else
                        {
                            WRITE(X_DIR_PIN,INVERT_X_DIR);
                        }
                        if(curd->dir & 2)
                        {
                            WRITE(Y_DIR_PIN,!INVERT_Y_DIR);
                        }
                        else
                        {
                            WRITE(Y_DIR_PIN,INVERT_Y_DIR);
                        }
                        if(curd->dir & 4)
                        {
                            WRITE(Z_DIR_PIN,!INVERT_Z_DIR);
                        }
                        else
                        {
                            WRITE(Z_DIR_PIN,INVERT_Z_DIR);
                        }
                    }
                    else
                    {
                        // Release the last segment
                        delta_segment_count--;
                        curd=0;
                    }
                }
            }
#if defined(USE_ADVANCE)
            if(printer.isAdvanceActivated()) // Use interrupt for movement
#endif
                Extruder::unstep();
        } // for loop
        if(do_odd)
        {
            HAL::allowInterrupts(); // Allow interrupts for other types, timer1 is still disabled
#ifdef RAMP_ACCELERATION
            //If acceleration is enabled on this move and we are in the acceleration segment, calculate the current interval
            if (cur->moveAccelerating())
            {
                printer.vMaxReached = HAL::ComputeV(printer.timer,PrintLine::cur->facceleration)+PrintLine::cur->vStart;
                if(printer.vMaxReached>PrintLine::cur->vMax) printer.vMaxReached = PrintLine::cur->vMax;
                unsigned int v;
                if(printer.vMaxReached>STEP_DOUBLER_FREQUENCY)
                {
#if ALLOW_QUADSTEPPING
                    if(printer.vMaxReached>STEP_DOUBLER_FREQUENCY*2)
                    {
                        printer.stepper_loops = 4;
                        v = printer.vMaxReached>>2;
                    }
                    else
                    {
                        printer.stepper_loops = 2;
                        v = printer.vMaxReached>>1;
                    }
#else
                    printer.stepper_loops = 2;
                    v = printer.vMaxReached>>1;
#endif
                }
                else
                {
                    printer.stepper_loops = 1;
                    v = printer.vMaxReached;
                }
                Printer::interval = HAL::CPUDivU2(v);
                printer.timer+=Printer::interval;
                cur->updateAdvanceSteps(printer.vMaxReached,max_loops,true);
            }
            else if (PrintLine::cur->moveDecelerating())     // time to slow down
            {
                unsigned int v = HAL::ComputeV(printer.timer,PrintLine::cur->facceleration);
                if (v > printer.vMaxReached)   // if deceleration goes too far it can become too large
                    v = PrintLine::cur->vEnd;
                else
                {
                    v=printer.vMaxReached-v;
                    if (v<PrintLine::cur->vEnd) v = PrintLine::cur->vEnd; // extra steps at the end of desceleration due to rounding erros
                }
                cur->updateAdvanceSteps(v,max_loops,false);
                if(v>STEP_DOUBLER_FREQUENCY)
                {
#if ALLOW_QUADSTEPPING
                    if(v>STEP_DOUBLER_FREQUENCY*2)
                    {
                        printer.stepper_loops = 4;
                        v = v>>2;
                    }
                    else
                    {
                        printer.stepper_loops = 2;
                        v = v>>1;
                    }
#else
                    printer.stepper_loops = 2;
                    v = v>>1;
#endif
                }
                else
                {
                    printer.stepper_loops = 1;
                }
                Printer::interval = HAL::CPUDivU2(v);
                printer.timer+=Printer::interval;
            }
            else
            {
                // If we had acceleration, we need to use the latest vMaxReached and interval
                // If we started full speed, we need to use cur->fullInterval and vMax
#ifdef USE_ADVANCE
                unsigned int v;
                if(!PrintLine::cur->accelSteps)
                {
                    v = PrintLine::cur->vMax;
                }
                else
                {
                    v = printer.vMaxReached;
                }
#ifdef ENABLE_QUADRATIC_ADVANCE
                long h=HAL::mulu16xu16to32(PrintLine::cur->advanceL,v);
                int tred = ((printer.advance_executed+h)>>16);
                HAL::forbidInterrupts();
                printer.extruderStepsNeeded+=tred-printer.advance_steps_set;
                printer.advance_steps_set = tred;
                HAL::allowInterrupts();
#else
                int tred=mulu6xu16shift16(PrintLine::cur->advanceL,v);
                HAL::forbidInterrupts();
                printer.extruderStepsNeeded+=tred-printer.advance_steps_set;
                printer.advance_steps_set = tred;
                HAL::allowInterrupts();
#endif
#endif
                if(!PrintLine::cur->accelSteps)
                {
                    if(PrintLine::cur->vMax>STEP_DOUBLER_FREQUENCY)
                    {
#if ALLOW_QUADSTEPPING
                        if(PrintLine::cur->vMax>STEP_DOUBLER_FREQUENCY*2)
                        {
                            printer.stepper_loops = 4;
                            Printer::interval = PrintLine::cur->fullInterval>>2;
                        }
                        else
                        {
                            printer.stepper_loops = 2;
                            Printer::interval = PrintLine::cur->fullInterval>>1;
                        }
#else
                        printer.stepper_loops = 2;
                        Printer::interval = PrintLine::cur->fullInterval>>1;
#endif
                    }
                    else
                    {
                        printer.stepper_loops = 1;
                        Printer::interval = PrintLine::cur->fullInterval;
                    }
                }
            }
#else
            Printer::interval = PrintLine::cur->fullInterval; // without RAMPS always use full speed
#endif
        } // do_odd
        if(do_even)
        {
            printer.stepNumber+=max_loops;
            PrintLine::cur->stepsRemaining-=max_loops;
        }

    } // stepsRemaining
    long interval;
    if(PrintLine::cur->halfstep)
        interval = (Printer::interval>>1);		// time to come back
    else
        interval = Printer::interval;
    if(do_even)
    {
        if(PrintLine::cur->stepsRemaining<=0 || (PrintLine::cur->dir & 240)==0)   // line finished
        {
//			out.println_int_P(PSTR("Line finished: "), (int) PrintLine::cur->numDeltaSegments);
//			out.println_int_P(PSTR("DSC: "), (int) delta_segment_count);
//			out.println_P(PSTR("F"));

            // Release remaining delta segments
            delta_segment_count -= PrintLine::cur->numDeltaSegments;
#ifdef DEBUG_STEPCOUNT
            if(PrintLine::cur->totalStepsRemaining)
            {
                out.println_long_P(PSTR("Missed steps:"), PrintLine::cur->totalStepsRemaining);
                out.println_long_P(PSTR("Step/seg r:"), stepsPerSegRemaining);
                out.println_int_P(PSTR("NDS:"), (int) PrintLine::cur->numDeltaSegments);
                out.println_int_P(PSTR("HS:"), (int) PrintLine::cur->halfstep);
            }
#endif
            PrintLine::popLineForbidInterrupt();
            if(DISABLE_X) Printer::disableXStepper();
            if(DISABLE_Y) Printer::disableYStepper();
            if(DISABLE_Z) Printer::disableZStepper();
            if(lines_count==0) UI_STATUS(UI_TEXT_IDLE);
            interval = Printer::interval = interval>>1; // 50% of time to next call to do cur=0
        }
        DEBUG_MEMORY;
    } // Do even
    return interval;
}
#else
/**
  Moves the stepper motors one step. If the last step is reached, the next movement is started.
  The function must be called from a timer loop. It returns the time for the next call.

  Normal non delta algorithm
*/
int lastblk=-1;
long cur_errupd;
long PrintLine::bresenhamStep()
{
    if(cur == 0)
    {
        HAL::allowInterrupts();
        ANALYZER_ON(ANALYZER_CH0);
        cur = &lines[lines_pos];
        if(PrintLine::cur->isBlocked())   // This step is in computation - shouldn't happen
        {
            if(lastblk!=(int)cur)
            {
                lastblk = (int)cur;
                out.println_int_P(PSTR("BLK "),(unsigned int)lines_count);
            }
            cur = 0;
            return 2000;
        }
        lastblk = -1;
#ifdef INCLUDE_DEBUG_NO_MOVE
        if(DEBUG_NO_MOVES)   // simulate a move, but do nothing in reality
        {
            PrintLine::popLineForbidInterrupt();
            return 1000;
        }
#endif
        ANALYZER_OFF(ANALYZER_CH0);
        if(PrintLine::cur->isWarmUp())
        {
            // This is a warmup move to initalize the path planner correctly. Just waste
            // a bit of time to get the planning up to date.
            if(lines_count<=PrintLine::cur->primaryAxis)
            {
                cur=0;
                return 2000;
            }
            long wait = PrintLine::cur->getWaitTicks();
            PrintLine::popLineForbidInterrupt();
            return(wait); // waste some time for path optimization to fill up
        } // End if WARMUP
        /*if(DEBUG_ECHO) {
        OUT_P_L_LN("MSteps:",cur->stepsRemaining);
          //OUT_P_F("Ln:",PrintLine::cur->startSpeed);
        //OUT_P_F_LN(":",PrintLine::cur->endSpeed);
        }*/
        //Only enable axis that are moving. If the axis doesn't need to move then it can stay disabled depending on configuration.
#ifdef XY_GANTRY
        if(PrintLine::cur->isXOrYMove())
        {
            Printer::enableXStepper();
            Printer::enableYStepper();
        }
#else
        if(PrintLine::cur->isXMove()) Printer::enableXStepper();
        if(PrintLine::cur->isYMove()) Printer::enableYStepper();
#endif
        if(PrintLine::cur->isZMove())
        {
            Printer::enableZStepper();
        }
        if(PrintLine::cur->isEMove()) Extruder::enable();
        PrintLine::cur->fixStartAndEndSpeed();
        HAL::allowInterrupts();
        if(PrintLine::cur->halfstep)
        {
            cur_errupd = PrintLine::cur->delta[PrintLine::cur->primaryAxis]<<1;
        }
        else
            cur_errupd = PrintLine::cur->delta[PrintLine::cur->primaryAxis];
        if(!PrintLine::cur->areParameterUpToDate())  // should never happen, but with bad timings???
        {
            cur->updateStepsParameter();
        }
        printer.vMaxReached = PrintLine::cur->vStart;
        printer.stepNumber=0;
        printer.timer = 0;
        HAL::forbidInterrupts();
        //Determine direction of movement,check if endstop was hit
#if !defined(XY_GANTRY)
        if(PrintLine::cur->dir & 1)
        {
            WRITE(X_DIR_PIN,!INVERT_X_DIR);
        }
        else
        {
            WRITE(X_DIR_PIN,INVERT_X_DIR);
        }
        if(PrintLine::cur->dir & 2)
        {
            WRITE(Y_DIR_PIN,!INVERT_Y_DIR);
        }
        else
        {
            WRITE(Y_DIR_PIN,INVERT_Y_DIR);
        }
#else
        long gdx = (PrintLine::cur->dir & 1 ? PrintLine::cur->delta[0] : -PrintLine::cur->delta[0]); // Compute signed difference in steps
        long gdy = (PrintLine::cur->dir & 2 ? PrintLine::cur->delta[1] : -PrintLine::cur->delta[1]);
#if DRIVE_SYSTEM==1
        if(gdx+gdy>=0)
        {
            WRITE(X_DIR_PIN,!INVERT_X_DIR);
            ANALYZER_ON(ANALYZER_CH4);
        }
        else
        {
            WRITE(X_DIR_PIN,INVERT_X_DIR);
            ANALYZER_OFF(ANALYZER_CH4);
        }
        if(gdx>gdy)
        {
            WRITE(Y_DIR_PIN,!INVERT_Y_DIR);
            ANALYZER_ON(ANALYZER_CH5);
        }
        else
        {
            WRITE(Y_DIR_PIN,INVERT_Y_DIR);
            ANALYZER_OFF(ANALYZER_CH5);
        }
#endif
#if DRIVE_SYSTEM==2
        if(gdx+gdy>=0)
        {
            WRITE(X_DIR_PIN,!INVERT_X_DIR);
        }
        else
        {
            WRITE(X_DIR_PIN,INVERT_X_DIR);
        }
        if(gdx<=gdy)
        {
            WRITE(Y_DIR_PIN,!INVERT_Y_DIR);
        }
        else
        {
            WRITE(Y_DIR_PIN,INVERT_Y_DIR);
        }
#endif
#endif
        if(PrintLine::cur->dir & 4)
        {
            WRITE(Z_DIR_PIN,!INVERT_Z_DIR);
        }
        else
        {
            WRITE(Z_DIR_PIN,INVERT_Z_DIR);
        }
#if defined(USE_ADVANCE)
        if(!printer.isAdvanceActivated()) // Set direction if no advance/OPS enabled
#endif
            if(PrintLine::cur->dir & 8)
            {
                Extruder::setDirection(1);
            }
            else
            {
                Extruder::setDirection(0);
            }
#ifdef USE_ADVANCE
        long h = HAL::mulu16xu16to32(PrintLine::cur->vStart,PrintLine::cur->advanceL);
        int tred = ((
#ifdef ENABLE_QUADRATIC_ADVANCE
                        (printer.advance_executed = PrintLine::cur->advanceStart)+
#endif
                        h)>>16);
        printer.extruderStepsNeeded+=tred-printer.advance_steps_set;
        printer.advance_steps_set = tred;
#endif
        if(printer.waslasthalfstepping && PrintLine::cur->halfstep==0)   // Switch halfstepping -> full stepping
        {
            printer.waslasthalfstepping = 0;
            return Printer::interval*3; // Wait an other 150% from last half step to make the 100% full
        }
        else if(!printer.waslasthalfstepping && PrintLine::cur->halfstep)     // Switch full to half stepping
        {
            printer.waslasthalfstepping = 1;
        }
        else
            return Printer::interval; // Wait an other 50% from last step to make the 100% full
    } // End cur=0
    HAL::allowInterrupts();
    /* For halfstepping, we divide the actions into even and odd actions to split
       time used per loop. */
    byte do_even;
    byte do_odd;
    if(PrintLine::cur->halfstep)
    {
        do_odd = PrintLine::cur->halfstep & 1;
        do_even = PrintLine::cur->halfstep & 2;
        PrintLine::cur->halfstep = 3-PrintLine::cur->halfstep;
    }
    else
    {
        do_even = 1;
        do_odd = 1;
    }
    HAL::forbidInterrupts();
    if(do_even)
    {
        if(PrintLine::cur->checkEndstops())
        {
            if(PrintLine::cur->isXNegativeMove() && Printer::isXMinEndstopHit())
                PrintLine::cur->setXMoveFinished();
            if(PrintLine::cur->isYNegativeMove() && Printer::isYMinEndstopHit())
                PrintLine::cur->setYMoveFinished();
            if(PrintLine::cur->isXPositiveMove() && Printer::isXMaxEndstopHit())
                PrintLine::cur->setXMoveFinished();
            if(PrintLine::cur->isYPositiveMove() && Printer::isYMaxEndstopHit())
                PrintLine::cur->setYMoveFinished();
        }
        // Test Z-Axis every step if necessary, otherwise it could easyly ruin your printer!
        if(PrintLine::cur->isZNegativeMove() && Printer::isZMinEndstopHit())
            PrintLine::cur->setZMoveFinished();
        if(PrintLine::cur->isZPositiveMove() && Printer::isZMaxEndstopHit())
            PrintLine::cur->setZMoveFinished();
    }
    byte max_loops = min(printer.stepper_loops,PrintLine::cur->stepsRemaining);
    if(PrintLine::cur->stepsRemaining>0)
    {
        for(byte loop=0; loop<max_loops; loop++)
        {
            ANALYZER_ON(ANALYZER_CH1);
            if(loop>0)
#if STEPPER_HIGH_DELAY>0
                HAL::delayMicroseconds(STEPPER_HIGH_DELAY+DOUBLE_STEP_DELAY);
#else
                HAL::delayMicroseconds(DOUBLE_STEP_DELAY);
#endif
            if(PrintLine::cur->isEMove())
            {
                if((PrintLine::cur->error[3] -= PrintLine::cur->delta[3]) < 0)
                {
#if defined(USE_ADVANCE)
                    if(printer.isAdvanceActivated())   // Use interrupt for movement
                    {
                        if(PrintLine::cur->isEPositiveMove())
                            printer.extruderStepsNeeded++;
                        else
                            printer.extruderStepsNeeded--;
                    }
                    else
                    {
#endif
                        Extruder::step();
#if defined(USE_ADVANCE)
                    }
#endif
                    PrintLine::cur->error[3] += cur_errupd;
                }
            }
#if defined(XY_GANTRY)
#endif
            if(PrintLine::cur->isXMove())
            {
                if((PrintLine::cur->error[0] -= PrintLine::cur->delta[0]) < 0)
                {
                    PrintLine::cur->startXStep();
                    PrintLine::cur->error[0] += cur_errupd;
                }
            }
            if(PrintLine::cur->isYMove())
            {
                if((PrintLine::cur->error[1] -= PrintLine::cur->delta[1]) < 0)
                {
                    PrintLine::cur->startYStep();
                    PrintLine::cur->error[1] += cur_errupd;
                }
            }
#if defined(XY_GANTRY)
            printer.executeXYGantrySteps();
#endif

            if(PrintLine::cur->isZMove())
            {
                if((PrintLine::cur->error[2] -= PrintLine::cur->delta[2]) < 0)
                {
                    WRITE(Z_STEP_PIN,HIGH);
                    PrintLine::cur->error[2] += cur_errupd;
#ifdef DEBUG_STEPCOUNT
                    PrintLine::cur->totalStepsRemaining--;
#endif
                }
            }
#if STEPPER_HIGH_DELAY>0
            HAL::delayMicroseconds(STEPPER_HIGH_DELAY);
#endif
#if defined(USE_ADVANCE)
            if(!printer.isAdvanceActivated()) // Use interrupt for movement
#endif
                Extruder::unstep();
            Printer::endXYZSteps();
        } // for loop
        if(do_odd)
        {
            HAL::allowInterrupts(); // Allow interrupts for other types, timer1 is still disabled
#ifdef RAMP_ACCELERATION
            //If acceleration is enabled on this move and we are in the acceleration segment, calculate the current interval
            if (PrintLine::cur->moveAccelerating())   // we are accelerating
            {
                printer.vMaxReached = HAL::ComputeV(printer.timer,PrintLine::cur->facceleration)+PrintLine::cur->vStart;
                if(printer.vMaxReached>PrintLine::cur->vMax) printer.vMaxReached = PrintLine::cur->vMax;
                unsigned int v;
                if(printer.vMaxReached>STEP_DOUBLER_FREQUENCY)
                {
#if ALLOW_QUADSTEPPING
                    if(printer.vMaxReached>STEP_DOUBLER_FREQUENCY*2)
                    {
                        printer.stepper_loops = 4;
                        v = printer.vMaxReached>>2;
                    }
                    else
                    {
                        printer.stepper_loops = 2;
                        v = printer.vMaxReached>>1;
                    }
#else
                    printer.stepper_loops = 2;
                    v = printer.vMaxReached>>1;
#endif
                }
                else
                {
                    printer.stepper_loops = 1;
                    v = printer.vMaxReached;
                }
                Printer::interval = HAL::CPUDivU2(v);
                printer.timer+=Printer::interval;
#ifdef USE_ADVANCE
                PrintLine::cur->updateAdvanceSteps(printer.vMaxReached,max_loops,true);
#endif
            }
            else if (PrintLine::cur->moveDecelerating())     // time to slow down
            {
                unsigned int v = HAL::ComputeV(printer.timer,PrintLine::cur->facceleration);
                if (v > printer.vMaxReached)   // if deceleration goes too far it can become too large
                    v = PrintLine::cur->vEnd;
                else
                {
                    v=printer.vMaxReached-v;
                    if (v<PrintLine::cur->vEnd) v = PrintLine::cur->vEnd; // extra steps at the end of desceleration due to rounding erros
                }
                PrintLine::cur->updateAdvanceSteps(v,max_loops,false); // needs original v
                if(v>STEP_DOUBLER_FREQUENCY)
                {
#if ALLOW_QUADSTEPPING
                    if(v>STEP_DOUBLER_FREQUENCY*2)
                    {
                        printer.stepper_loops = 4;
                        v = v>>2;
                    }
                    else
                    {
                        printer.stepper_loops = 2;
                        v = v>>1;
                    }
#else
                    printer.stepper_loops = 2;
                    v = v>>1;
#endif
                }
                else
                {
                    printer.stepper_loops = 1;
                }
                Printer::interval = HAL::CPUDivU2(v);
                printer.timer+=Printer::interval;
            }
            else
            {
                // constant speed reached
                if(PrintLine::cur->vMax>STEP_DOUBLER_FREQUENCY)
                {
#if ALLOW_QUADSTEPPING
                    if(PrintLine::cur->vMax>STEP_DOUBLER_FREQUENCY*2)
                    {
                        printer.stepper_loops = 4;
                        Printer::interval = PrintLine::cur->fullInterval>>2;
                    }
                    else
                    {
                        printer.stepper_loops = 2;
                        Printer::interval = PrintLine::cur->fullInterval>>1;
                    }
#else
                    printer.stepper_loops = 2;
                    Printer::interval = PrintLine::cur->fullInterval>>1;
#endif
                }
            }
#else
            Printer::interval = PrintLine::cur->fullInterval; // without RAMPS always use full speed
#endif
        } // do_odd
        if(do_even)
        {
            printer.stepNumber+=max_loops;
            PrintLine::cur->stepsRemaining-=max_loops;
        }

    } // stepsRemaining
    long interval;
    if(PrintLine::cur->halfstep) interval = (Printer::interval>>1); // time to come back
    else interval = Printer::interval;
    if(do_even)
    {
        if(PrintLine::cur->stepsRemaining<=0 || (PrintLine::cur->dir & 240)==0)   // line finished
        {
#ifdef DEBUG_STEPCOUNT
            if(PrintLine::cur->totalStepsRemaining)
                OUT_P_L_LN("Missed steps:",PrintLine::cur->totalStepsRemaining);
#endif

            HAL::forbidInterrupts();
            NEXT_PLANNER_INDEX(lines_pos);
            cur = 0;
            --lines_count;
#ifdef XY_GANTRY
            if(DISABLE_X && DISABLE_Y)
            {
                Printer::disableXStepper();
                Printer::disableYStepper();
            }
#else
            if(DISABLE_X) Printer::disableXStepper();
            if(DISABLE_Y) Printer::disableYStepper();
#endif
            if(DISABLE_Z) Printer::disableZStepper();
            if(lines_count==0) UI_STATUS(UI_TEXT_IDLE);
            interval = Printer::interval = interval>>1; // 50% of time to next call to do cur=0
        }
        DEBUG_MEMORY;
    } // Do even
    return interval;
}
#endif
void(* resetFunc) (void) = 0; //declare reset function @ address 0





