/*
 * Switch.cpp
 *
 * Created: 11/3/2017 2:10:47 PM
 *  Author: orencollaco
 */ 

#include "Switch.h"
#include "driver/gpio.h"

SwitchClass Switch;

SwitchClass *SwitchClass::Sptr[15];

uint8_t SwitchClass::i = 0;
uint8_t SwitchClass::SwitchState; 
uint8_t SwitchClass::PinBuffer[3];
bool SwitchClass::NotFirstSpawn = 0;
bool SwitchClass::AllSamePtr_EN = 0;
bool SwitchClass::AllShort_EN = 0;
bool SwitchClass::AllDouble_EN = 0;
bool SwitchClass::AllLong_EN = 0;
bool SwitchClass::AllFallingEdge_EN = 0;
bool SwitchClass::AllRisingEdge_EN = 0;
bool SwitchClass::AllContinuousLong_EN = 0;
Fptr SwitchClass::AllDoublePressPtr = NULL;
Fptr SwitchClass::AllLongPressPtr = NULL;
Fptr SwitchClass::AllContinuousLongPressPtr = NULL;
Fptr SwitchClass::AllShortPressPtr = NULL;
Fptr SwitchClass::AllRisingEdgePtr = NULL;
Fptr SwitchClass::AllFallingEdgePtr = NULL;
Fptr SwitchClass::PinStateChangePtr = NULL;

void SwitchClass::begin(){
	i = 0;
	PinStateChangePtr = doNothing;
}

void SwitchClass::doNothing(uint8_t data){
	
}

void SwitchClass::callOnPinStateChange(Fptr callback){
	PinStateChangePtr = callback;
}

uint8_t SwitchClass::getSwitchID(){
	return Switch_ID;
}

void SwitchClass::initializeSwitch(uint8_t pinNumber){
	ShortPressPtr = NULL;
	DoublePressPtr = NULL;
	LongPressPtr = NULL;
	ContinuousLongPressPtr = NULL;
	RisingEdgePtr = NULL;
	FallingEdgePtr = NULL;
	aTimer.initializeTimer();
	PinNumber = pinNumber;
	S = gpio_get_level((gpio_num_t)PinNumber);
	// PortNumber = portNumber;
	// setPinDirection(PortNumber, PinNumber, 0);
	// setPinState(PortNumber, PinNumber, 1);
	// setPinChangeInterrupt(portNumber, pinNumber, 1);
	// switch(PortNumber){
	// 	case 0:
	// 	SwitchState = PINB;
	// 	break;
	// 	case 1:
	// 	SwitchState = PINC;
	// 	break;
	// 	case 2:
	// 	SwitchState = PIND;
	// 	break;
	// }
	Sptr[i] = this;
	Switch_ID = i;
	i += 1;
	#ifdef DEBUG_SWITCH
	printStringCRNL("Initializing Switch : ");
	printNumber(Switch_ID);
	#endif
	//pollAllSwitches();
	// updatePinBuffers();
	pollSwitch();
	DoublePress_EN = 0;
	MyTimerID = aTimer.getTimerID();
}

void SwitchClass::fallingEdge(Fptr callback){
	FallingEdgePtr = callback;
	AllFallingEdgePtr = callback;
	FallingEdge_EN = 1;
}

void SwitchClass::risingEdge(Fptr callback){
	RisingEdgePtr = callback;
	AllRisingEdgePtr = callback;
	RisingEdge_EN = 1;
}

void SwitchClass::shortPress(Fptr callback){
	ShortPressPtr = callback;
	AllShortPressPtr = callback;
	ShortPress_EN = 1;
} 

void SwitchClass::longPress(Fptr callback){
	LongPressPtr = callback;
	AllLongPressPtr = callback;
	LongPress_EN = 1;
}

void SwitchClass::continuousLongPress(Fptr callback){
	ContinuousLongPressPtr = callback;
	AllContinuousLongPressPtr = callback;
	ContinuousLongPress_EN = 1;	
}

void SwitchClass::doublePress(Fptr callback){
	DoublePressPtr = callback;
	AllDoublePressPtr = callback;
	DoublePress_EN = 1;
}

void SwitchClass::processStateChange(){
	pollSwitch();
	#ifdef DEBUG_SWITCH
	printStringCRNL("Switch ID : ");
	printNumber(Switch_ID);
	printString(" State : ");
	printNumber(S);
	#endif
	if(!S && Old_S){
		#ifdef DEBUG_SWITCH
		printString("1->0");
		#endif
		if(FallingEdge_EN){
			if(FallingEdgePtr != NULL)
			FallingEdgePtr(Switch_ID);
		}
		if(!S_PressedOnce)
		aTimer.setCallBackTime(700, 0, callAllOjectLongWait);
		S_Pressed = 1;
		if(S_PressedOnce){
			aTimer.setTime(600);
			S_DoublePressed = 1;
			S_PressedOnce = 0;
		}
	}
	if(S && !Old_S){
		#ifdef DEBUG_SWITCH
		printString("0->1");
		#endif 
		if(RisingEdge_EN){
			if(RisingEdgePtr != NULL)
			RisingEdgePtr(Switch_ID);
		}
		if(!S_DoublePressed)
		TimePassed = aTimer.getCallBackTime();
		else
		TimePassed = aTimer.getTime();
		aTimer.resetTimer();
		aTimer.resetCallbackTimer();
		S_Pressed = 0;
		if(TimePassed > 10 && TimePassed < 600){
			if(!S_DoublePressed && !S_LongPressed){
				if(DoublePress_EN){
					aTimer.setCallBackTime(100, 0, callAllDoubleWait);
					S_PressedOnce = 1;
				}
				else{
					//printChar('i');
					doubleWaitComplete();
				}
			}
			if(S_DoublePressed){
				#ifdef DEBUG_SWITCH
				printStringCRNL("Double Pressed : ");
				printNumber(Switch_ID);
				#endif
				if(AllSamePtr_EN){
					//allowSleep(1);
					if(AllDoublePressPtr != NULL)
					AllDoublePressPtr(Switch_ID);
				}
				else{
					//allowSleep(1);
					if(DoublePressPtr != NULL)
					DoublePressPtr(Switch_ID);
				}	
			}
		}
		if(S_LongPressed){
			S_LongPressed = 0;
			#ifdef DEBUG_SWITCH
			printStringCRNL("Continuous Long Press Ended : ");
			printNumber(Switch_ID);
			#endif
		}
		if(S_DoublePressed)
		S_DoublePressed = 0;
	}
}

void SwitchClass::doubleWaitComplete(){
	S_PressedOnce = 0;
	S_DoublePressed = 0;
	#ifdef DEBUG_SWITCH
	printStringCRNL("Short Pressed : ");
	printNumber(Switch_ID);
	#endif 
	aTimer.resetTimer();
	aTimer.resetCallbackTimer();
	if(ShortPress_EN || AllShort_EN){
		if(AllSamePtr_EN){
			//allowSleep(1);
			if(AllShortPressPtr != NULL)
			AllShortPressPtr(Switch_ID);
		}
		else{
			//allowSleep(1);
			if(ShortPressPtr != NULL)
			ShortPressPtr(Switch_ID);
		}
	}
}

void SwitchClass::longWaitComplete(){
		#ifdef DEBUG_SWITCH
		printStringCRNL("Long Wait Complete : ");
		printNumber(Switch_ID);
		#endif
		Timer_EN = 0;
		S_PressedOnce = 0;
		S_DoublePressed = 0;
		if(S_LongPressed){
			pollSwitch();
			if(!S){
				if(ContinuousLongPress_EN || AllContinuousLong_EN){
					#ifdef DEBUG_SWITCH
					printStringCRNL("Continuous Long Press : ");
					printNumber(Switch_ID);
					#endif
					aTimer.setCallBackTime(70, 0, callAllOjectLongWait);
					if(AllSamePtr_EN){
						//allowSleep(1);
						if(AllContinuousLongPressPtr != NULL)
						AllContinuousLongPressPtr(Switch_ID);
					}
					else{
						//allowSleep(1);
						if(ContinuousLongPressPtr != NULL)
						ContinuousLongPressPtr(Switch_ID);
					}
				}
			}
		}
		if(S_Pressed){
			//updatePinBuffers();
			pollSwitch();
			if(!S){
				if(LongPress_EN || AllLong_EN){
					#ifdef DEBUG_SWITCH
					printStringCRNL("Long Pressed : ");
					printNumber(Switch_ID);
					printNumber(aTimer.getTimerID());
					#endif 
					S_Pressed = 0;
					S_LongPressed = 1;
					aTimer.setCallBackTime(50, 0, callAllOjectLongWait);
					if(AllSamePtr_EN){
						//allowSleep(1);
						if(AllLongPressPtr != NULL)
						AllLongPressPtr(Switch_ID);
					}
					else{
						//allowSleep(1);
						if(LongPressPtr != NULL)
						LongPressPtr(Switch_ID);
					}
				}
			}
		}
}

void SwitchClass::callAllProcessStateChange(){
	#ifdef DEBUG_SWITCH
	printStringCRNL("Calling all process state change objects...");
	#endif 
	for(uint8_t a = 0; a < i; a += 1){
		if(Sptr[a] != NULL)
		Sptr[a]->processStateChange();
	}
}

void SwitchClass::callAllOjectLongWait(uint8_t timer_ID){
	for(uint8_t a = 0; a < i; a += 1){
		if(Sptr[a]->MyTimerID == timer_ID)
			if(Sptr[a] != NULL)
			Sptr[a]->longWaitComplete();
	}
}

void SwitchClass::callAllDoubleWait(uint8_t timer_ID){
	for(uint8_t a = 0; a < i; a += 1){
		if(Sptr[a]->MyTimerID == timer_ID)
			if(Sptr[a] != NULL)
			Sptr[a]->doubleWaitComplete();
	}
}

void SwitchClass::detectDoublePress(){
	
}

void SwitchClass::pollAllSwitches(){
	
}

void SwitchClass::enableSamePtrMode(bool set){
	AllSamePtr_EN = set;
	AllShort_EN = set;
	AllDouble_EN = set;
	AllLong_EN = set;
	AllContinuousLong_EN = set;
	AllFallingEdge_EN = set;
	AllRisingEdge_EN = set;
}

// void SwitchClass::updatePinBuffers(){
// 	PinBuffer[0] = PINB;
// 	PinBuffer[1] = PINC;
// 	PinBuffer[2] = PIND;
// }

void SwitchClass::pollSwitch(){
	// switch(PortNumber){
	// 	case 0:
	// 	SwitchState = PinBuffer[PortNumber];
	// 	break;
	// 	case 1:
	// 	SwitchState = PinBuffer[PortNumber];
	// 	break;
	// 	case 2:
	// 	SwitchState = PinBuffer[PortNumber];
	// 	break;
	// }
	Old_S = S;
	S = gpio_get_level((gpio_num_t)PinNumber);
}

void SwitchClass::pinStateChanged(){
	//printChar('s');
	Switch.callAllProcessStateChange();
}



// ISR(INT0_vect){
// 	Switch.PinBuffer[0] = PINB;
// 	Switch.PinBuffer[1] = PINC;
// 	Switch.PinBuffer[2] = PIND;
// 	wakeUp();
// 	#ifdef DEBUG_SWITCH
// 	printStringCRNL("0 S");
// 	#endif 
// 	allowSleep(0);
// 	Switch.pinStateChanged();
// }

// ISR(INT1_vect){
// 	wakeUp();
// 	#ifdef DEBUG_SWITCH
// 	printStringCRNL("1 S");
// 	#endif 
// 	allowSleep(0);
// 	Switch.pinStateChanged();
// }

// ISR(PCINT0_vect){
// 	//Switch.PinStateChangePtr(PORT_B);
// 	Switch.PinBuffer[0] = PINB;
// 	Switch.PinBuffer[1] = PINC;
// 	Switch.PinBuffer[2] = PIND;
// 	wakeUp();
// 	#ifdef DEBUG_SWITCH
// 	printStringCRNL("PinChanged -> PORT B");
// 	#endif
// 	allowSleep(0);
// 	Switch.pinStateChanged();
// }

// ISR(PCINT1_vect){
// 	//Switch.PinStateChangePtr(PORT_C);
// 	Switch.PinBuffer[0] = PINB;
// 	Switch.PinBuffer[1] = PINC;
// 	Switch.PinBuffer[2] = PIND;
// 	wakeUp();
// 	#ifdef DEBUG_SWITCH
// 	printStringCRNL("PinChanged -> PORT C");
// 	#endif	
// 	allowSleep(0);
// 	Switch.pinStateChanged();
// }

// ISR(PCINT2_vect){
// 	//Switch.PinStateChangePtr(PORT_D);
// 	Switch.PinBuffer[0] = PINB;
// 	Switch.PinBuffer[1] = PINC;
// 	Switch.PinBuffer[2] = PIND;
// 	wakeUp();
// 	#ifdef DEBUG_SWITCH
// 	printStringCRNL("PinChanged -> PORT D");
// 	#endif
// 	allowSleep(0);
// 	Switch.pinStateChanged();
// }

