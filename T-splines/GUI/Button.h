#ifndef BUTTON_H
#define BUTTON_H

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include "Common/Common.h" 


class Button : public Fl_Button{
public: 
	Button(int x, int y, int w, int h, const char* l=0): 
         Fl_Button(x,y,w,h,l){
			 this->color(WIN_COLOR); 
			 this->box(FL_BORDER_BOX); 
    }
}; 

#endif 