/PROG  LDR_MTC_I
/ATTR
OWNER		= MNEDITOR;
COMMENT		= "Load MTConnect/ROS-I";
PROG_SIZE	= 176;
CREATE		= DATE 12-10-02  TIME 12:08:46;
MODIFIED	= DATE 12-10-02  TIME 12:08:46;
FILE_NAME	= ;
VERSION		= 0;
LINE_COUNT	= 5;
MEMORY_SIZE	= 556;
PROTECT		= READ_WRITE;
TCD:  STACK_SIZE	= 0,
      TASK_PRIORITY	= 50,
      TIME_SLICE	= 0,
      BUSY_LAMP_OFF	= 0,
      ABORT_REQUEST	= 0,
      PAUSE_REQUEST	= 0;
DEFAULT_GROUP	= *,*,*,*,*;
CONTROL_CODE	= 00000000 00000000;
/MN
   1:  RUN ROS_STATE ;
   2:  RUN ROS_MOVESM ;
   3:  RUN ROS_GRIPPER;
   4:  RUN ROS_VISE;
   5:  CALL ROS_RELAY    ;
/POS
/END
