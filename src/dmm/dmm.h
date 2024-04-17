#ifndef _DMM_H__
#define _DMM_H__

typedef struct
{
  int r,g,b;
} color_t;

typedef struct
{
  char Text[255];
  color_t  Color;
} status_t;


///////////////////////////////////////////// FUNCTION PROTOTYPES ///////////////////////////////

void GetConfigParam(char *, char *, char *);
void SetConfigParam(char *, char *, char *);
int CheckWebCtlExists();
void ReadSavedParams();
void *WaitTouchscreenEvent(void * arg);
void *WebClickListener(void * arg);
void parseClickQuerystring(char *query_string, int *x_ptr, int *y_ptr);
FFUNC touchscreenClick(ffunc_session_t * session);
void UpdateWeb();
void do_snapcheck();
int IsImageToBeChanged(int x,int y);
void Keyboard(char *, char *, int);
int openTouchScreen(int);
int getTouchScreenDetails(int*, int* ,int* ,int*);
void TransformTouchMap(int x, int y);
int IsButtonPushed(int NbButton, int x, int y);
int IsMenuButtonPushed(int x, int y);
int InitialiseButtons();
int AddButton(int x, int y, int w, int h);
int ButtonNumber(int, int);
int CreateButton(int MenuIndex, int ButtonPosition);
int AddButtonStatus(int ButtonIndex,char *Text,color_t *Color);
void AmendButtonStatus(int ButtonIndex, int ButtonStatusIndex, char *Text, color_t *Color);
void DrawButton(int ButtonIndex);
void SetButtonStatus(int ,int);
int GetButtonStatus(int ButtonIndex);
int getTouchSampleThread(int *rawX, int *rawY, int *rawPressure);
int getTouchSample(int *rawX, int *rawY, int *rawPressure);
void UpdateWindow();
void wait_touch();
void CalculateMarkers();
void ChangeLabel(int);
void ChangeSpan(int menu, int button);
void ChangePlot(int button);
void ChangeDischarge(int button);
void CalculateCapacityUsed(float InstantaneousVoltage, uint64_t MeasurementPeriod);
void CheckDischargeLimit(float InstantaneousVoltage);
void SaveCSV();
void ResetCSVFile();
void *WaitButtonEvent(void * arg);
void Define_Menu1();                 // Main Menu
void Define_Menu2();                 // Marker Menu
void Define_Menu3();                 // Settings Menu
void Define_Menu4();                 // System Menu
void Define_Menu5();                 // Mode Menu
void Define_Menu6();                 // Not used
void Define_Menu7();                 // Set-up Discharge Menu
void Define_Menu8();                 // Not used
void Define_Menu9();                 // Actions Menu
void Define_Menu10();                 // Span in Minutes
void Define_Menu11();                 // Span in Hours
void Start_Highlights_Menu1();
void Start_Highlights_Menu2();
void Start_Highlights_Menu3();
void Start_Highlights_Menu4();
void Start_Highlights_Menu5();
void Start_Highlights_Menu6();
void Start_Highlights_Menu7();
void Start_Highlights_Menu8();
void Start_Highlights_Menu9();
void Start_Highlights_Menu10();
void Start_Highlights_Menu11();
void Define_Menu41();
void DrawEmptyScreen();
void DrawYaxisLabels();
void DrawTitle();
void DrawBaselineText();
void DrawTrace(int xoffset, int prev2, int prev1, int current);
void displayByte(uint8_t input);
void *serial_listener(void * arg);
void *serial_decoder(void * arg);
void DecodeSerialSentence();
static void cleanexit(int calling_exit_code);
static void terminate(int dummy);


typedef struct
{
  int adc_val;
  float pwr_dBm;
} adc_lookup;


int ad8318_3_underrange = 633;
int ad8318_3_overrange = 161;



#endif /* _DMM_H__ */
