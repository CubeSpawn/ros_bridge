/PROG  LDR_VICE
/ATTR
OWNER		= MNEDITOR;
COMMENT		= "Load Gripper";
PROG_SIZE	= 154;
CREATE		= DATE 12-10-02  TIME 12:08:46;
MODIFIED	= DATE 12-10-02  TIME 12:08:46;
FILE_NAME	= ;
VERSION		= 0;
LINE_COUNT	= 1;
MEMORY_SIZE	= 530;
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
   1:  CALL ROS_VISE    ;
/POS
/END

