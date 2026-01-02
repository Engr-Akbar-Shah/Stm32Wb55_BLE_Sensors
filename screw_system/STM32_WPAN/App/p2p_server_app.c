/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    p2p_server_app.c
  * @author  MCD Application Team
  * @brief   Peer to peer Server Application
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"
#include "p2p_server_app.h"
#include "stm32_seq.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
 typedef struct{
    uint8_t             Device_Led_Selection;
    uint8_t             Led1;
 }P2P_LedCharValue_t;

 typedef struct{
    uint8_t             Device_Button_Selection;
    uint8_t             ButtonStatus;
 }P2P_ButtonCharValue_t;

typedef struct
{
  uint8_t               Notification_Status; /* used to check if P2P Server is enabled to Notify */
  P2P_LedCharValue_t    LedControl;
  P2P_ButtonCharValue_t ButtonControl;
  uint16_t              ConnectionHandle;
  /*My sensor config*/
  uint8_t               LSM6DSO_Active;
  uint8_t               STTSH22H_Active;
  uint8_t               BothSensors_Active;
  uint8_t               LSM6DSO_Timer_Id;
  uint8_t               STTSH22H_Timer_Id;
} P2P_Server_App_Context_t;
/* USER CODE END PTD */

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE BEGIN PD */

#define VERSION_REQUEST_PREFIX      0x30
#define NOTIF_VERSION_RESPONSE      0x30

/* Firmware version - change these numbers as needed */
#define FW_VERSION_MAJOR            2
#define FW_VERSION_MINOR            0
#define FW_VERSION_PATCH            0

#define SENSOR_DATA_INTERVAL  (100)  /* 100ms = 10Hz update rate */

/* Sensor command protocol */
#define SENSOR_CMD_PREFIX     0x10
#define SENSOR_LSM6DSO        0x01
#define SENSOR_STTSH22H       0x02
#define SENSOR_BOTH           0x03
#define SENSOR_START          0x01
#define SENSOR_STOP           0x00

/* Notification data format */
#define NOTIF_SENSOR_DATA     0x20
#define NOTIF_SENSOR_STATUS   0x21
/* USER CODE END PD */
/* USER CODE END PD */

/* Private macros -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/**
 * START of Section BLE_APP_CONTEXT
 */

static P2P_Server_App_Context_t P2P_Server_App_Context;

/**
 * END of Section BLE_APP_CONTEXT
 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void P2PS_Send_Notification(void);
static void P2PS_APP_LED_BUTTON_context_Init(void);
static void P2PS_Send_LSM6DSO_Data(void);
static void P2PS_Send_STTSH22H_Data(void);
static void P2PS_Start_Sensor(uint8_t sensor_id);
static void P2PS_Stop_Sensor(uint8_t sensor_id);
static void P2PS_Send_Version_Response(void);
static void P2PS_Send_Sensor_Status(uint8_t sensor_id, uint8_t status);
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void P2PS_STM_App_Notification(P2PS_STM_App_Notification_evt_t *pNotification)
{
/* USER CODE BEGIN P2PS_STM_App_Notification_1 */

/* USER CODE END P2PS_STM_App_Notification_1 */
  switch(pNotification->P2P_Evt_Opcode)
  {
/* USER CODE BEGIN P2PS_STM_App_Notification_P2P_Evt_Opcode */
#if(BLE_CFG_OTA_REBOOT_CHAR != 0)
    case P2PS_STM_BOOT_REQUEST_EVT:
      APP_DBG_MSG("-- P2P APPLICATION SERVER : BOOT REQUESTED\n\r");
      APP_DBG_MSG(" \n\r");

      *(uint32_t*)SRAM1_BASE = *(uint32_t*)pNotification->DataTransfered.pPayload;
      NVIC_SystemReset();
      break;
#endif
/* USER CODE END P2PS_STM_App_Notification_P2P_Evt_Opcode */

    case P2PS_STM__NOTIFY_ENABLED_EVT:
/* USER CODE BEGIN P2PS_STM__NOTIFY_ENABLED_EVT */
      P2P_Server_App_Context.Notification_Status = 1;
      APP_DBG_MSG("-- P2P APPLICATION SERVER : NOTIFICATION ENABLED\n\r");
      APP_DBG_MSG(" \n\r");
/* USER CODE END P2PS_STM__NOTIFY_ENABLED_EVT */
      break;

    case P2PS_STM_NOTIFY_DISABLED_EVT:
/* USER CODE BEGIN P2PS_STM_NOTIFY_DISABLED_EVT */
      P2P_Server_App_Context.Notification_Status = 0;
      APP_DBG_MSG("-- P2P APPLICATION SERVER : NOTIFICATION DISABLED\n\r");
      APP_DBG_MSG(" \n\r");
      /* ADD THIS LINE: */
      P2PS_Stop_Sensor(SENSOR_BOTH);
/* USER CODE END P2PS_STM_NOTIFY_DISABLED_EVT */
      break;

    case P2PS_STM_WRITE_EVT:
/* USER CODE BEGIN P2PS_STM_WRITE_EVT */
    	APP_DBG_MSG("-- WRITE EVENT RECEIVED: Length=%d Byte0=0x%02X\n\r",
    	                pNotification->DataTransfered.Length,
    	                pNotification->DataTransfered.pPayload[0]);
    	if(pNotification->DataTransfered.Length >= 3 &&
    	     pNotification->DataTransfered.pPayload[0] == SENSOR_CMD_PREFIX)
    	  {
    	    uint8_t sensor_id = pNotification->DataTransfered.pPayload[1];
    	    uint8_t action = pNotification->DataTransfered.pPayload[2];

    	    APP_DBG_MSG("-- SENSOR COMMAND: Sensor=0x%02X Action=0x%02X\n\r", sensor_id, action);

    	    if(action == SENSOR_START)
    	    {
    	      P2PS_Start_Sensor(sensor_id);
    	    }
    	    else if(action == SENSOR_STOP)
    	    {
    	      P2PS_Stop_Sensor(sensor_id);
    	    }

    	    /* Send status confirmation */
    	    P2PS_Send_Sensor_Status(sensor_id, action);
    	  }

        else if (pNotification->DataTransfered.Length >= 1 &&
                 pNotification->DataTransfered.pPayload[0] == VERSION_REQUEST_PREFIX)
        {
          APP_DBG_MSG("-- VERSION: Request received (0x30)\n\r");
          P2PS_Send_Version_Response();
        }

    	else if(pNotification->DataTransfered.pPayload[0] == 0x00){ /* ALL Deviceselected - may be necessary as LB Routeur informs all connection */
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
          BSP_LED_On(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER  : LED1 ON\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x01; /* LED1 ON */
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
          BSP_LED_Off(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER  : LED1 OFF\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x00; /* LED1 OFF */
        }
      }
#if(P2P_SERVER1 != 0)  
      if(pNotification->DataTransfered.pPayload[0] == 0x01){ /* end device 1 selected - may be necessary as LB Routeur informs all connection */
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
          BSP_LED_On(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 1 : LED1 ON\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x01; /* LED1 ON */
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
          BSP_LED_Off(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 1 : LED1 OFF\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x00; /* LED1 OFF */
        }
      }
#endif
#if(P2P_SERVER2 != 0)
      if(pNotification->DataTransfered.pPayload[0] == 0x02){ /* end device 2 selected */ 
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
          BSP_LED_On(LED_BLUE);
           APP_DBG_MSG("-- P2P APPLICATION SERVER 2 : LED1 ON\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x01; /* LED1 ON */
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
          BSP_LED_Off(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 2 : LED1 OFF\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x00; /* LED1 OFF */
        }   
      }
#endif      
#if(P2P_SERVER3 != 0)  
      if(pNotification->DataTransfered.pPayload[0] == 0x03){ /* end device 3 selected - may be necessary as LB Routeur informs all connection */
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
          BSP_LED_On(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 3 : LED1 ON\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x01; /* LED1 ON */
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
          BSP_LED_Off(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 3 : LED1 OFF\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x00; /* LED1 OFF */
        }
      }
#endif
#if(P2P_SERVER4 != 0)
      if(pNotification->DataTransfered.pPayload[0] == 0x04){ /* end device 4 selected */ 
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
          BSP_LED_On(LED_BLUE);
           APP_DBG_MSG("-- P2P APPLICATION SERVER 2 : LED1 ON\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x01; /* LED1 ON */
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
          BSP_LED_Off(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 2 : LED1 OFF\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x00; /* LED1 OFF */
        }   
      }
#endif     
#if(P2P_SERVER5 != 0)  
      if(pNotification->DataTransfered.pPayload[0] == 0x05){ /* end device 5 selected - may be necessary as LB Routeur informs all connection */
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
          BSP_LED_On(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 5 : LED1 ON\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x01; /* LED1 ON */
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
          BSP_LED_Off(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 5 : LED1 OFF\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x00; /* LED1 OFF */
        }
      }
#endif
#if(P2P_SERVER6 != 0)
      if(pNotification->DataTransfered.pPayload[0] == 0x06){ /* end device 6 selected */ 
        if(pNotification->DataTransfered.pPayload[1] == 0x01)
        {
          BSP_LED_On(LED_BLUE);
           APP_DBG_MSG("-- P2P APPLICATION SERVER 6 : LED1 ON\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x01; /* LED1 ON */
        }
        if(pNotification->DataTransfered.pPayload[1] == 0x00)
        {
          BSP_LED_Off(LED_BLUE);
          APP_DBG_MSG("-- P2P APPLICATION SERVER 6 : LED1 OFF\n\r");
          APP_DBG_MSG(" \n\r");
          P2P_Server_App_Context.LedControl.Led1=0x00; /* LED1 OFF */
        }   
      }
#endif 
/* USER CODE END P2PS_STM_WRITE_EVT */
      break;

    default:
/* USER CODE BEGIN P2PS_STM_App_Notification_default */
      
/* USER CODE END P2PS_STM_App_Notification_default */
      break;
  }
/* USER CODE BEGIN P2PS_STM_App_Notification_2 */

/* USER CODE END P2PS_STM_App_Notification_2 */
  return;
}

void P2PS_APP_Notification(P2PS_APP_ConnHandle_Not_evt_t *pNotification)
{
/* USER CODE BEGIN P2PS_APP_Notification_1 */

/* USER CODE END P2PS_APP_Notification_1 */
  switch(pNotification->P2P_Evt_Opcode)
  {
/* USER CODE BEGIN P2PS_APP_Notification_P2P_Evt_Opcode */

/* USER CODE END P2PS_APP_Notification_P2P_Evt_Opcode */
  case PEER_CONN_HANDLE_EVT :
/* USER CODE BEGIN PEER_CONN_HANDLE_EVT */
          
/* USER CODE END PEER_CONN_HANDLE_EVT */
    break;

    case PEER_DISCON_HANDLE_EVT :
/* USER CODE BEGIN PEER_DISCON_HANDLE_EVT */
       P2PS_APP_LED_BUTTON_context_Init();

       P2PS_Stop_Sensor(SENSOR_BOTH);
/* USER CODE END PEER_DISCON_HANDLE_EVT */
    break;

    default:
/* USER CODE BEGIN P2PS_APP_Notification_default */

/* USER CODE END P2PS_APP_Notification_default */
      break;
  }
/* USER CODE BEGIN P2PS_APP_Notification_2 */

/* USER CODE END P2PS_APP_Notification_2 */
  return;
}

void P2PS_APP_Init(void)
{
/* USER CODE BEGIN P2PS_APP_Init */
  UTIL_SEQ_RegTask( 1<< CFG_TASK_SW1_BUTTON_PUSHED_ID, UTIL_SEQ_RFU, P2PS_Send_Notification );
  UTIL_SEQ_RegTask( 1<< CFG_TASK_SEND_LSM6DSO_ID, UTIL_SEQ_RFU, P2PS_Send_LSM6DSO_Data );
  UTIL_SEQ_RegTask( 1<< CFG_TASK_SEND_STTSH22H_ID, UTIL_SEQ_RFU, P2PS_Send_STTSH22H_Data );


  /**
   * Initialize LedButton Service
   */
  P2P_Server_App_Context.Notification_Status=0; 
  P2PS_APP_LED_BUTTON_context_Init();

  P2P_Server_App_Context.LSM6DSO_Active = 0;
  P2P_Server_App_Context.STTSH22H_Active = 0;
  P2P_Server_App_Context.BothSensors_Active = 0;

  HW_TS_Create(CFG_TIM_PROC_ID_ISR, &(P2P_Server_App_Context.LSM6DSO_Timer_Id),
                 hw_ts_Repeated, P2PS_Send_LSM6DSO_Data);
  HW_TS_Create(CFG_TIM_PROC_ID_ISR, &(P2P_Server_App_Context.STTSH22H_Timer_Id),
                 hw_ts_Repeated, P2PS_Send_STTSH22H_Data);

    APP_DBG_MSG("-- P2P SERVER: Sensor system initialized\n\r");
/* USER CODE END P2PS_APP_Init */
  return;
}

/* USER CODE BEGIN FD */
void P2PS_APP_LED_BUTTON_context_Init(void){
  
  BSP_LED_Off(LED_BLUE);
  
  #if(P2P_SERVER1 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x01; /* Device1 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x01;/* Device1 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif
#if(P2P_SERVER2 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x02; /* Device2 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x02;/* Device2 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif  
#if(P2P_SERVER3 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x03; /* Device3 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x03; /* Device3 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif
#if(P2P_SERVER4 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x04; /* Device4 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x04; /* Device4 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif  
 #if(P2P_SERVER5 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x05; /* Device5 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x05; /* Device5 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif
#if(P2P_SERVER6 != 0)
  P2P_Server_App_Context.LedControl.Device_Led_Selection=0x06; /* device6 */
  P2P_Server_App_Context.LedControl.Led1=0x00; /* led OFF */
  P2P_Server_App_Context.ButtonControl.Device_Button_Selection=0x06; /* Device6 */
  P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
#endif  
}

void P2PS_APP_SW1_Button_Action(void)
{
  UTIL_SEQ_SetTask( 1<<CFG_TASK_SW1_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);

  return;
}
/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
/* USER CODE BEGIN FD_LOCAL_FUNCTIONS*/
void P2PS_Send_Notification(void)
{
 
  if(P2P_Server_App_Context.ButtonControl.ButtonStatus == 0x00){
    P2P_Server_App_Context.ButtonControl.ButtonStatus=0x01;
  } else {
    P2P_Server_App_Context.ButtonControl.ButtonStatus=0x00;
  }
  
   if(P2P_Server_App_Context.Notification_Status){ 
    APP_DBG_MSG("-- P2P APPLICATION SERVER  : INFORM CLIENT BUTTON 1 PUSHED \n\r");
    APP_DBG_MSG(" \n\r");
    P2PS_STM_App_Update_Char(P2P_NOTIFY_CHAR_UUID, (uint8_t *)&P2P_Server_App_Context.ButtonControl);
   } else {
    APP_DBG_MSG("-- P2P APPLICATION SERVER : CAN'T INFORM CLIENT -  NOTIFICATION DISABLED\n\r");
   }

  return;
}

static void P2PS_Start_Sensor(uint8_t sensor_id)
{
  uint32_t interval_ticks = (SENSOR_DATA_INTERVAL * 1000) / CFG_TS_TICK_VAL;

  switch(sensor_id)
  {
    case SENSOR_LSM6DSO:
      if(!P2P_Server_App_Context.BothSensors_Active)
      {
        P2P_Server_App_Context.LSM6DSO_Active = 1;
        HW_TS_Start(P2P_Server_App_Context.LSM6DSO_Timer_Id, interval_ticks);
        APP_DBG_MSG("-- SENSOR: LSM6DSO Started (10Hz)\n\r");
      }
      break;

    case SENSOR_STTSH22H:
      if(!P2P_Server_App_Context.BothSensors_Active)
      {
        P2P_Server_App_Context.STTSH22H_Active = 1;
        HW_TS_Start(P2P_Server_App_Context.STTSH22H_Timer_Id, interval_ticks);
        APP_DBG_MSG("-- SENSOR: STTSH22H Started (10Hz)\n\r");
      }
      break;

    case SENSOR_BOTH:
      P2P_Server_App_Context.BothSensors_Active = 1;
      P2P_Server_App_Context.LSM6DSO_Active = 0;
      P2P_Server_App_Context.STTSH22H_Active = 0;

      HW_TS_Start(P2P_Server_App_Context.LSM6DSO_Timer_Id, interval_ticks);
      HW_TS_Start(P2P_Server_App_Context.STTSH22H_Timer_Id, interval_ticks);
      APP_DBG_MSG("-- SENSOR: BOTH Sensors Started (10Hz)\n\r");
      break;
  }
}

static void P2PS_Stop_Sensor(uint8_t sensor_id)
{
  switch(sensor_id)
  {
    case SENSOR_LSM6DSO:
      P2P_Server_App_Context.LSM6DSO_Active = 0;
      HW_TS_Stop(P2P_Server_App_Context.LSM6DSO_Timer_Id);
      APP_DBG_MSG("-- SENSOR: LSM6DSO Stopped\n\r");
      break;

    case SENSOR_STTSH22H:
      P2P_Server_App_Context.STTSH22H_Active = 0;
      HW_TS_Stop(P2P_Server_App_Context.STTSH22H_Timer_Id);
      APP_DBG_MSG("-- SENSOR: STTSH22H Stopped\n\r");
      break;

    case SENSOR_BOTH:
      P2P_Server_App_Context.LSM6DSO_Active = 0;
      P2P_Server_App_Context.STTSH22H_Active = 0;
      P2P_Server_App_Context.BothSensors_Active = 0;

      HW_TS_Stop(P2P_Server_App_Context.LSM6DSO_Timer_Id);
      HW_TS_Stop(P2P_Server_App_Context.STTSH22H_Timer_Id);
      APP_DBG_MSG("-- SENSOR: All Sensors Stopped\n\r");
      break;
  }
}

static void P2PS_Send_Sensor_Status(uint8_t sensor_id, uint8_t status)
{
  if(P2P_Server_App_Context.Notification_Status)
  {
    uint8_t payload[4];
    payload[0] = NOTIF_SENSOR_STATUS;
    payload[1] = sensor_id;
    payload[2] = status;
    payload[3] = 0x00;

    P2PS_STM_App_Update_Char(P2P_NOTIFY_CHAR_UUID, payload);
    APP_DBG_MSG("-- SENSOR STATUS: ID=0x%02X Status=0x%02X sent\n\r", sensor_id, status);
  }
}

static void P2PS_Send_LSM6DSO_Data(void)
{
  if(!P2P_Server_App_Context.Notification_Status)
    return;

  if(!P2P_Server_App_Context.LSM6DSO_Active && !P2P_Server_App_Context.BothSensors_Active)
    return;

  static int16_t accel_x = 100, accel_y = -50, accel_z = 1000;
  static int16_t gyro_x = 5, gyro_y = -3, gyro_z = 1;

  uint8_t payload[14];
  payload[0] = NOTIF_SENSOR_DATA;
  payload[1] = SENSOR_LSM6DSO;

  accel_x = 100 + (rand() % 20 - 10);
  accel_y = -50 + (rand() % 10 - 5);
  accel_z = 1000 + (rand() % 20 - 10);

  payload[2] = (accel_x >> 8) & 0xFF;
  payload[3] = accel_x & 0xFF;
  payload[4] = (accel_y >> 8) & 0xFF;
  payload[5] = accel_y & 0xFF;
  payload[6] = (accel_z >> 8) & 0xFF;
  payload[7] = accel_z & 0xFF;

  gyro_x = 5 + (rand() % 6 - 3);
  gyro_y = -3 + (rand() % 6 - 3);
  gyro_z = 1 + (rand() % 4 - 2);

  payload[8] = (gyro_x >> 8) & 0xFF;
  payload[9] = gyro_x & 0xFF;
  payload[10] = (gyro_y >> 8) & 0xFF;
  payload[11] = gyro_y & 0xFF;
  payload[12] = (gyro_z >> 8) & 0xFF;
  payload[13] = gyro_z & 0xFF;

  P2PS_STM_App_Update_Char(P2P_NOTIFY_CHAR_UUID, payload);
}

static void P2PS_Send_STTSH22H_Data(void)
{
  if(!P2P_Server_App_Context.Notification_Status)
    return;

  if(!P2P_Server_App_Context.STTSH22H_Active && !P2P_Server_App_Context.BothSensors_Active)
    return;

  static int16_t temperature = 2500;

  uint8_t payload[4];
  payload[0] = NOTIF_SENSOR_DATA;
  payload[1] = SENSOR_STTSH22H;

  temperature = 2450 + (rand() % 100);

  payload[2] = (temperature >> 8) & 0xFF;
  payload[3] = temperature & 0xFF;

  P2PS_STM_App_Update_Char(P2P_NOTIFY_CHAR_UUID, payload);
}

static void P2PS_Send_Version_Response(void)
{
  if (P2P_Server_App_Context.Notification_Status)
  {
    uint8_t payload[4];
    payload[0] = NOTIF_VERSION_RESPONSE;
    payload[1] = FW_VERSION_MAJOR;
    payload[2] = FW_VERSION_MINOR;
    payload[3] = FW_VERSION_PATCH;

    P2PS_STM_App_Update_Char(P2P_NOTIFY_CHAR_UUID, payload);

    APP_DBG_MSG("-- VERSION: Sent %d.%d.%d\n\r",
                FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
  }
}

/* USER CODE END FD_LOCAL_FUNCTIONS*/
