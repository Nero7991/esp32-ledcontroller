/*
 * Switch.h
 *
 * Created: 11/3/2017 2:10:34 PM
 *  Author: orencollaco
 */ 


#ifndef SWITCH_H_
#define SWITCH_H_

////#include "Essential.h"
#include "Timer.h"
#include "driver/gpio.h"

class SwitchClass{
	
 public:
	
		static uint8_t SwitchState, i, PinBuffer[3];
		
		static bool NotFirstSpawn, AllSamePtr_EN, AllShort_EN, AllLong_EN, AllDouble_EN, AllContinuousLong_EN, AllFallingEdge_EN, AllRisingEdge_EN;
		
		static SwitchClass *Sptr[15];
		
		static Fptr AllShortPressPtr, AllLongPressPtr, AllDoublePressPtr, AllContinuousLongPressPtr, AllRisingEdgePtr, AllFallingEdgePtr, PinStateChangePtr;
		
		static void begin();
		
		static void callOnPinStateChange(Fptr);
		
		static void doNothing(uint8_t);
		
		static void callAllOjectLongWait(uint8_t timer_ID);
		
		static void callAllProcessStateChange();
		
		static void callAllDoubleWait(uint8_t timer_ID);
		
		static void pollAllSwitches();
		
		static void enableSamePtrMode(bool set);
		
		static void pinStateChanged();
		
		TimerClass aTimer;
		
		Fptr ShortPressPtr, LongPressPtr, DoublePressPtr, ContinuousLongPressPtr, RisingEdgePtr, FallingEdgePtr; 
		
		bool S, S_EN, ShortPress_EN, LongPress_EN, ContinuousLongPress_EN, DoublePress_EN, Timer_EN, FallingEdge_EN, RisingEdge_EN;
		
		bool Old_S, S_Pressed, S_PressedOnce, S_DoublePressed, S_LongPressed;
		
		uint8_t Switch_ID, PinNumber, PortNumber, MyTimerID;
		
		uint16_t timeSincePressedOnce, TimePassed;;
		
		void initializeSwitch(uint8_t pinNumber, SwitchClass *sptr);
		
		void pollSwitch();
		
		//void updatePinBuffers();
		
		void longWaitComplete();
		
		void doubleWaitComplete();
		
		void fallingEdge(Fptr callback);
		
		void risingEdge(Fptr callback);
		
		void shortPress(Fptr callback);
		
		void longPress(Fptr callback);
		
		void continuousLongPress(Fptr callback);
		
		void doublePress(Fptr callback);
		
		void detectShortPress();
		
		void detectLongPress();
		
		void detectDoublePress();
		
		void processStateChange();
		
		uint8_t getSwitchID();
		
};

extern SwitchClass Switch;

#endif /* SWITCH_H_ */