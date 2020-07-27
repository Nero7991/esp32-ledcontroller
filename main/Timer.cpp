/*
 * Timer.cpp
 *
 * Created: 11/3/2017 2:55:58 PM
 *  Author: orencollaco
 */ 

#include "Timer.h"

TimerClass Timer;

TimerClass *TimerClass::Timers[50];

uint64_t TimerClass::Time = 0;

uint8_t TimerClass::i = 1;


void TimerClass::begin(){
	i = 1;
}

void TimerClass::initializeTimer(){
	Timers[i] = this;
	Timer_ID = i;
	i += 1;
	#ifdef DEBUG_TIMER
	printStringCRNL("Timer initialized : ");
	printNumber(Timer_ID);
	#endif
}

void TimerClass::setCallBackTime(uint32_t time, uint8_t mode, Fptr func){
	Count = time;
	Count_Save = time;
	CallBackPtr = func;
	Count_EN = 1;
	Mode = mode;
	//allowSleep(0);
}


void TimerClass::resetCallbackTimer(){
	Count_EN = 0;
	Count = 0;	
}

uint16_t TimerClass::getCallBackTime(){
	return Count_Save - Count;
}

void TimerClass::setTime(uint16_t time){
	NCount = time;
	NCount_EN = 1;
	NCount_Save = time;
	//allowSleep(0);
}

uint16_t TimerClass::getTime(){
	return NCount_Save - NCount;
}

void TimerClass::resetTimer(){
	NCount = 0;
	NCount_EN = 0;
}


uint64_t TimerClass::millis(){
	return Time;
}

void TimerClass::milliPassed(){
	if(Count_EN){
		#ifdef DEBUG_TIMER
		//printChar('T');
		#endif
		if(Count)
		Count -= 1;
		if(!Count){
			if(Mode == TIMER_MODE_REPEAT){
				Count = Count_Save;
			}
			if(Mode == TIMER_MODE_ONCE){
				Count_EN = 0;
				Count = 0;
			}
			if(CallBackPtr != NULL){
				//#ifdef DEBUG_TIMER
				//printStringCRNL("Timer Done : ");
				//printNumber(Timer_ID);
				//#endif
				//allowSleep(1);
				CallBackPtr(Timer_ID);
			}
		}
	}
	if(NCount_EN){
		if(NCount)
		NCount -= 1;
		if(!NCount){
			NCount_EN = 0;
		}
	}
}

void TimerClass::callAllTimerObjects(){
	for(uint8_t a = 0; a < i; a += 1){
		if(Timers[a] != NULL)
		Timers[a]->milliPassed();
	}
}

uint8_t TimerClass::getTimerID(){
	return Timer_ID;
}


void TimerClass::milliHappened(uint8_t IntArg){
	//printChar('I');
	Timer.Time += 1;
	Timer.callAllTimerObjects();
	//printChar('O');
}

void TimerClass::allowSleep(bool Enable){

}
