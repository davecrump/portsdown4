#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "Graphics.h"
#include "Touch.h"


void processTouch();
void initGUI();
void detectHw();
int buttonTouched(int bx,int by);


#define testButtonsY 429
#define testButtonsX 30
#define exitButtonX 30
#define exitButtonY 10
#define textX 200
#define textY 200

#define buttonHeight 50
#define buttonSpaceY 55
#define buttonWidth 100
#define buttonSpaceX 105


char touchPath[21];
int touchPresent;


int main(int argc, char* argv[])
{
  detectHw();
  initScreen();
  if(touchPresent) initTouch(touchPath);

  BackgroundRGB(255, 0, 255, 255);
  
  initGUI();  
  
  
  while(1)
  { 
   if(touchPresent)
     {
       if(getTouch()==1)
        {
          processTouch();
        }
     } 
  }

}




void detectHw()
{
	FILE * fp;
	char * ln=NULL;
	size_t len=0;
	ssize_t rd;
	int p;
	char handler[10];
	char * found;
	p=0;
	touchPresent=0;
	fp=fopen("/proc/bus/input/devices","r");
	 while ((rd=getline(&ln,&len,fp)!=-1))
	  {
	    if(ln[0]=='N')        //name of device
	    {
	      if(strstr(ln,"FT5406")!=NULL) p=1; else p=0;     //Found Raspberry Pi TouchScreen entry
	    }
	    
	    if ((p==1) && (ln[0]=='H'))        // found handler
	    {
	       if(strstr(ln,"mouse")!=NULL)
	       {
	         found=strstr(ln,"event");
	         strcpy(handler,found);
	         handler[6]=0;
	           sprintf(touchPath,"/dev/input/%s",handler);
	           touchPresent=1;
	       }
	    }   
	  }
	fclose(fp);
	if(ln)  free(ln);
}



void initGUI()
{
  //clearScreen();
  gotoXY(testButtonsX,testButtonsY);
  setForeColour(0,255,0);
  displayButton("1");
  displayButton("2");
  displayButton("3");
  displayButton("4");
  displayButton("5");
  displayButton("6");
  displayButton("7");
  
  gotoXY(exitButtonX,exitButtonY);
  displayButton("Exit");
}





void processTouch()
{ 

if(buttonTouched(testButtonsX,testButtonsY))    //Button 1
    {
    	gotoXY(textX,textY);
    	setForeColour(255,0,0);
    	textSize=3;
    	displayStr("Button 1 Pressed");
      return;         
    }      
if(buttonTouched(testButtonsX+buttonSpaceX,testButtonsY))    //Button 2
    {
      gotoXY(textX,textY);
    	setForeColour(255,0,0);
    	textSize=3;
    	displayStr("Button 2 Pressed");
      return;
    }
      
if(buttonTouched(testButtonsX+buttonSpaceX*2,testButtonsY))  // Button 3 
    {
      gotoXY(textX,textY);
    	setForeColour(255,0,0);
    	textSize=3;
    	displayStr("Button 3 Pressed");
      return;
    }
      
if(buttonTouched(testButtonsX+buttonSpaceX*3,testButtonsY))    // Button 4
    {
      gotoXY(textX,textY);
    	setForeColour(255,0,0);
    	textSize=3;
    	displayStr("Button 4 Pressed");
      return;
    }
       
if(buttonTouched(testButtonsX+buttonSpaceX*4,testButtonsY))    //Button 5
    {
    	gotoXY(textX,textY);
    	setForeColour(255,0,0);
    	textSize=3;
    	displayStr("Button 5 Pressed");
      return;     
    }      

if(buttonTouched(testButtonsX+buttonSpaceX*5,testButtonsY))    //Button 6 
    {
      gotoXY(textX,textY);
    	setForeColour(255,0,0);
    	textSize=3;
    	displayStr("Button 6 Pressed");
      return;
    } 
		     
if(buttonTouched(testButtonsX+buttonSpaceX*6,testButtonsY))   //Button 7
    {
    	gotoXY(textX,textY);
    	setForeColour(255,0,0);
    	textSize=3;
    	displayStr("Button 7 Pressed");
      return;
    }      

  if(buttonTouched(exitButtonX,exitButtonY))
  {
    setBackColour(0, 0, 0);
    clearScreen();
    exit(0);
  }
}


int buttonTouched(int bx,int by)
{
  return ((touchX > bx) & (touchX < bx+buttonWidth) & (touchY > by) & (touchY < by+buttonHeight));
}


