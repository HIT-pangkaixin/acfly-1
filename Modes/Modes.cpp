#include "Basic.hpp"
#include "Modes.hpp"
#include "FreeRTOS.h"
#include "task.h"
#include "Receiver.hpp"
#include "Commulink.hpp"
#include "MeasurementSystem.hpp"
#include "StorageSystem.hpp"
#include "Parameters.hpp"
#include "queue.h"
#include "mavlink.h"
#include "drv_PWMOut.hpp"
#include "AuxFuncs.hpp"
#include "ControlSystem.hpp"

#include "M10_RCCalib.hpp"
#include "M11_TempCalib.hpp"
#include "M12_AccCalib.hpp"
#include "M13_MagCalib.hpp"

#include "M30_AttCtrl.hpp"
#include "M32_PosCtrl.hpp"
#include "M35_Auto1.hpp"

Mode_Base* modes[80] = {0};
static QueueHandle_t message_queue = xQueueCreate( 20, sizeof(ModeMsg) );
bool SendMsgToMode( ModeMsg msg, double TIMEOUT )
{
	TickType_t TIMEOUT_Ticks;
	if( TIMEOUT >= 0 )
		TIMEOUT_Ticks = TIMEOUT*configTICK_RATE_HZ;
	else
		TIMEOUT_Ticks = portMAX_DELAY;
	if( xQueueSend( message_queue, &msg, TIMEOUT_Ticks ) == pdTRUE )
		return true;
	else
		return false;
}
bool ModeReceiveMsg( ModeMsg* msg, double TIMEOUT )
{
	TickType_t TIMEOUT_Ticks;
	if( TIMEOUT >= 0 )
		TIMEOUT_Ticks = TIMEOUT*configTICK_RATE_HZ;
	else
		TIMEOUT_Ticks = portMAX_DELAY;
	if( xQueueReceive( message_queue, msg, TIMEOUT_Ticks ) == pdTRUE )
		return true;
	else
		return false;
}
static bool changeMode( uint16_t mode_index, void* param1, uint32_t param2, ModeResult* result )
{
	if( modes[mode_index] != 0 )
	{					
		xQueueReset(message_queue);
		if( result != 0 )
			*result = modes[mode_index]->main_func( param1, param2 );
		else
			modes[mode_index]->main_func( param1, param2 );
		xQueueReset(message_queue);
		return true;
	}
	return false;
}

static void Modes_Server(void* pvParameters)
{
	//�ȴ�������ʼ�����
	while( getInitializationCompleted() == false )
		os_delay(0.1);
	setLedManualCtrl( 0, 0, 30, true, 800 );
	os_delay(0.2);
	setLedMode(LEDMode_Processing1);
	
	//�ȴ���̬����ϵͳ׼�����
	while( get_Attitude_MSStatus() != MS_Ready )
		os_delay(0.1);
	setLedManualCtrl( 0, 0, 30, true, 1000 );
	os_delay(0.2);
	setLedMode(LEDMode_Processing1);
	
	//�ȴ�λ�ý���ϵͳ׼�����
	while( get_Altitude_MSStatus() != MS_Ready )
		os_delay(0.1);
	setLedManualCtrl( 0, 0, 30, true, 1200 );
	os_delay(0.2);
	setLedMode(LEDMode_Processing1);
	sendLedSignal(LEDSignal_Start2);

	//��ʼ��Aux����
	init_process_AuxFuncs();
	
	//�������ģʽ
	xQueueReset(message_queue);
	set_mav_mode( 
		MAV_MODE_STABILIZE_DISARMED | MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
		PX4_CUSTOM_MAIN_MODE_STABILIZED,
		0 );
	setLedMode(LEDMode_Normal1);
	uint16_t pre_enter_mode_counter = 0;
	uint8_t last_pre_enter_mode = 0;
	
	//״̬
	uint8_t ModeButtonZone = 255;
	uint8_t ModeLock = 0;	//1-����ģʽ
	while(1)
	{
		os_delay(0.02);
		
		//�رտ�����
		Attitude_Control_Disable();
		
		//��ȡ���ջ�
		Receiver rc;
		getReceiver( &rc, 0, 0.02);

		if( rc.available )
		{	//���ջ����ø���ģʽ��ť״̬
			uint8_t new_ModeButtonZone = get_RcButtonZone( rc.data[4], ModeButtonZone );
			if( ModeButtonZone<=5 && new_ModeButtonZone!=ModeButtonZone )
			{	//ģʽ��ť�ı���������״̬
				ModeLock = 0;
			}
			ModeButtonZone = new_ModeButtonZone;
		}
		else
		{	//���ջ�����������ң��״̬
			ModeButtonZone = 255;
		}
		
		//����Auxͨ��
		process_AuxFuncs(&rc);
		
		//��ȡ��Ϣ
		bool msg_available;
		ModeMsg msg;
		msg_available = ModeReceiveMsg( &msg, 0 );
		
		//��ȡģʽ����
		ModeFuncCfg MFunc_cfg;
		ReadParamGroup( "MFunc", (uint64_t*)&MFunc_cfg, 0 );
		
		//��ȡ��λ״̬
		if( get_Position_MSStatus() == MS_Ready )
			setLedMode(LEDMode_Normal2);
		else
			setLedMode(LEDMode_Normal1);
		
		//ģʽ�ش���ʾ
		if( ModeLock==0 )
		{
			uint8_t p_mode = 0;
			if( ModeButtonZone==0 )
				p_mode = MFunc_cfg.Bt1PAFunc1[0];
			else if( ModeButtonZone==1 )
				p_mode = MFunc_cfg.Bt1PAFunc2[0];
			else if( ModeButtonZone==2 )
				p_mode = MFunc_cfg.Bt1PAFunc3[0];
			else if( ModeButtonZone==3 )
				p_mode = MFunc_cfg.Bt1PAFunc4[0];
			else if( ModeButtonZone==4 )
				p_mode = MFunc_cfg.Bt1PAFunc5[0];
			else
				p_mode = MFunc_cfg.Bt1PAFunc6[0];
			
			if( modes[p_mode] != 0 )
			{
				uint16_t mav_mode, mav_main_mode, mav_sub_mode;
				modes[p_mode]->get_MavlinkMode( MFunc_cfg, rc, &mav_mode, &mav_main_mode, &mav_sub_mode );
				set_mav_mode( 
					mav_mode,
					mav_main_mode,
					mav_sub_mode );
			}
		}
		else if( ModeLock==1 )
		{
			set_mav_mode( 
				MAV_MODE_STABILIZE_DISARMED | MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
				PX4_CUSTOM_MAIN_MODE_AUTO,
				PX4_CUSTOM_SUB_MODE_AUTO_MISSION );
		}
		
		if( rc.available )
		{
			uint8_t pre_enter_mode = 0;
			
			if( (rc.data[0] < 10.0f) && (rc.data[1] < 10.0f) && (rc.data[2] < 10.0f) && (rc.data[3] < 10.0f) )
				pre_enter_mode = 12;	//���ٶ�У׼
			else if( (rc.data[0] < 10.0f) && (rc.data[1] < 10.0f) && (rc.data[2] > 90.0f) && (rc.data[3] < 10.0f) )
				pre_enter_mode = 13;	//������У׼
			else if( (rc.data[0] < 10.0f) && (rc.data[1] > 90.0f) && (rc.data[2] > 45.0f) && (rc.data[2] < 55.0f) && (rc.data[3] > 90.0f) )
				pre_enter_mode = 11;	//�¶�ϵ��У׼
			else if( (rc.data[0] < 10.0f) && (rc.data[1] > 90.0f) && (rc.data[2] < 10.0f) && (rc.data[3] < 10.0f) )
			{
				if( ModeLock==0 )
				{
					if( ModeButtonZone==0 )
						pre_enter_mode = MFunc_cfg.Bt1PAFunc1[0];
					else if( ModeButtonZone==1 )
						pre_enter_mode = MFunc_cfg.Bt1PAFunc2[0];
					else if( ModeButtonZone==2 )
						pre_enter_mode = MFunc_cfg.Bt1PAFunc3[0];
					else if( ModeButtonZone==3 )
						pre_enter_mode = MFunc_cfg.Bt1PAFunc4[0];
					else if( ModeButtonZone==4 )
						pre_enter_mode = MFunc_cfg.Bt1PAFunc5[0];
					else
						pre_enter_mode = MFunc_cfg.Bt1PAFunc6[0];
				}
				else if( ModeLock==1 )
					pre_enter_mode = 32;
				else
					pre_enter_mode = 0;
			}
			
			//��������ģʽ
			if( pre_enter_mode==0 || pre_enter_mode!=last_pre_enter_mode )
				pre_enter_mode_counter = 0;
			else
			{
				if( ++pre_enter_mode_counter >= 50 )
				{
					if( modes[pre_enter_mode] != 0 )
					{
						sendLedSignal(LEDSignal_Start1);
						if( ModeLock==1 )
						{	//�����Զ�ģʽ
							sendLedSignal(LEDSignal_Start1);
							px4_custom_mode t_mav_mode;
							t_mav_mode.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
							t_mav_mode.sub_mode = PX4_CUSTOM_SUB_MODE_AUTO_MISSION;
							changeMode( 32, 0, t_mav_mode.data, 0 );
							ModeLock = 0;
						}
						else
							changeMode( pre_enter_mode, 0, 0, 0 );
						//ģʽִ����Ϸ��ر�ģʽ
						last_pre_enter_mode = 0;
						continue;
					}
				}				
			}
			last_pre_enter_mode = pre_enter_mode;
		}
		
		//������Ϣ
		if( msg_available )
		{
			switch( msg.cmd )
			{
				case 176:
				{	//do set mode
					sendLedSignal(LEDSignal_Start1);
					if( msg.params[0] == 0 )
						changeMode( msg.params[1], (void*)(uint32_t)msg.params[3], msg.params[2], 0 );
					else if( (int)msg.params[0] & MAV_MODE_FLAG_CUSTOM_MODE_ENABLED )
					{	//mavlink����ģʽ
						px4_custom_mode t_mav_mode;
						t_mav_mode.data = msg.params[1];
						if( (t_mav_mode.main_mode==PX4_CUSTOM_MAIN_MODE_AUTO || t_mav_mode.main_mode==0) && t_mav_mode.sub_mode==PX4_CUSTOM_SUB_MODE_AUTO_MISSION )
						{	//�Զ�ģʽ
							if( (msg.cmd_type & CMD_TYPE_MASK) == CMD_TYPE_MAVLINK )
							{
								uint8_t port_index = msg.cmd_type & CMD_TYPE_PORT_MASK;
								const Port* port = get_Port( port_index );
								if( port->write )
								{
									mavlink_message_t msg_sd;
									if( mavlink_lock_chan( port_index, 0.01 ) )
									{
										mavlink_msg_command_ack_pack_chan( 
											get_CommulinkSysId() ,	//system id
											get_CommulinkCompId() ,	//component id
											port_index ,
											&msg_sd,
											msg.cmd,	//command
											MAV_RESULT_ACCEPTED ,	//result
											100 ,	//progress
											0 ,	//param2
											msg.sd_sysid ,	//target system
											msg.sd_compid //target component
										);
										mavlink_msg_to_send_buffer(port->write, 
																							 port->lock,
																							 port->unlock,
																							 &msg_sd, 0, 0.01);
										mavlink_unlock_chan(port_index);
									}
								}
							}
							if( (int)msg.params[0] & MAV_MODE_FLAG_SAFETY_ARMED )
							{
								sendLedSignal(LEDSignal_Start1);
								changeMode( 32, 0, t_mav_mode.data, 0 );
								ModeLock = 0;
							}
							else
								ModeLock = 1;
						}
						else if( t_mav_mode.main_mode==PX4_CUSTOM_MAIN_MODE_POSCTL || t_mav_mode.main_mode==PX4_CUSTOM_MAIN_MODE_ALTCTL || 
										 t_mav_mode.main_mode==PX4_CUSTOM_MAIN_MODE_STABILIZED || t_mav_mode.main_mode==PX4_CUSTOM_MAIN_MODE_ACRO )
						{	//�ֶ�ģʽ
							if( (msg.cmd_type & CMD_TYPE_MASK) == CMD_TYPE_MAVLINK )
							{
								uint8_t port_index = msg.cmd_type & CMD_TYPE_PORT_MASK;
								const Port* port = get_Port( port_index );
								if( port->write )
								{
									mavlink_message_t msg_sd;
									if( mavlink_lock_chan( port_index, 0.01 ) )
									{
										mavlink_msg_command_ack_pack_chan( 
											get_CommulinkSysId() ,	//system id
											get_CommulinkCompId() ,	//component id
											port_index ,
											&msg_sd,
											msg.cmd,	//command
											MAV_RESULT_ACCEPTED ,	//result
											100 ,	//progress
											0 ,	//param2
											msg.sd_sysid ,	//target system
											msg.sd_compid //target component
										);
										mavlink_msg_to_send_buffer(port->write, 
																							 port->lock,
																							 port->unlock,
																							 &msg_sd, 0, 0.01);
										mavlink_unlock_chan(port_index);
									}
								}
							}
							ModeLock = 0;
						}
					}
					//ģʽִ����Ϸ��ر�ģʽ
					setLedMode(LEDMode_Normal1);
					last_pre_enter_mode = 0;
					break;
				}
				
				case 22:
				{	//takeoff���
					px4_custom_mode t_mav_mode;
					t_mav_mode.reserved = msg.frame;
					t_mav_mode.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
					t_mav_mode.sub_mode = PX4_CUSTOM_SUB_MODE_AUTO_TAKEOFF;
					if( (msg.cmd_type & CMD_TYPE_MASK) == CMD_TYPE_MAVLINK )
					{
						uint8_t port_index = msg.cmd_type & CMD_TYPE_PORT_MASK;
						const Port* port = get_Port( port_index );
						if( port->write )
						{
							mavlink_message_t msg_sd;
							if( mavlink_lock_chan( port_index, 0.01 ) )
							{
								mavlink_msg_command_ack_pack_chan( 
									get_CommulinkSysId() ,	//system id
									get_CommulinkCompId() ,	//component id
									port_index ,
									&msg_sd,
									msg.cmd,	//command
									MAV_RESULT_ACCEPTED ,	//result
									100 ,	//progress
									0 ,	//param2
									msg.sd_sysid ,	//target system
									msg.sd_compid //target component
								);
								mavlink_msg_to_send_buffer(port->write, 
																					 port->lock,
																					 port->unlock,
																					 &msg_sd, 0, 0.01);sendLedSignal(LEDSignal_Start1);
								mavlink_unlock_chan(port_index);
							}
						}
					}
					
					changeMode( 32, msg.params, t_mav_mode.data, 0 );
					ModeLock = 0;
					break;
				}
				
				case MAV_CMD_COMPONENT_ARM_DISARM:
				{	//����
					if( msg.params[0] == 1 )
					{
						if( (msg.cmd_type & CMD_TYPE_MASK) == CMD_TYPE_MAVLINK )
						{
							uint8_t port_index = msg.cmd_type & CMD_TYPE_PORT_MASK;
							const Port* port = get_Port( port_index );
							if( port->write )
							{
								mavlink_message_t msg_sd;
								if( mavlink_lock_chan( port_index, 0.01 ) )
								{
									mavlink_msg_command_ack_pack_chan( 
										get_CommulinkSysId() ,	//system id
										get_CommulinkCompId() ,	//component id
										port_index ,
										&msg_sd,
										msg.cmd,	//command
										MAV_RESULT_ACCEPTED ,	//result
										100 ,	//progress
										0 ,	//param2
										msg.sd_sysid ,	//target system
										msg.sd_compid //target component
									);
									mavlink_msg_to_send_buffer(port->write, 
																						 port->lock,
																						 port->unlock,
																						 &msg_sd, 0, 0.01);
									mavlink_unlock_chan(port_index);
								}
							}
						}
						sendLedSignal(LEDSignal_Start1);
						if( ModeLock==1 )
						{	//�����Զ�ģʽ
							sendLedSignal(LEDSignal_Start1);
							px4_custom_mode t_mav_mode;
							t_mav_mode.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
							t_mav_mode.sub_mode = PX4_CUSTOM_SUB_MODE_AUTO_MISSION;
							changeMode( 32, 0, t_mav_mode.data, 0 );
							ModeLock = 0;
						}
						else
							changeMode( 32, 0, 0, 0 );
						ModeLock = 0;
					}
					break;
				}
				
			}
		}
	}
}

void ModeRegister( Mode_Base* mode, uint8_t id )
{
	if( modes[id] == 0 )
		modes[id] = mode;
}

void init_Modes()
{
	//ע��ģʽ
	new M10_RCCalib();
	new M11_TempCalib();
	new M12_AccCalib();
	new M13_MagCalib();
	
	new M30_AttCtrl();
	new M32_PosCtrl();
	new M35_Auto1();
	
	//ע�����
	ModeFuncCfg initial_cfg;
	//��ť1����ǰ���ܣ�ģʽ��ţ�
	initial_cfg.Bt1PAFunc1[0] = 32;
	initial_cfg.Bt1PAFunc2[0] = 32;
	initial_cfg.Bt1PAFunc3[0] = 35;
	initial_cfg.Bt1PAFunc4[0] = 35;
	initial_cfg.Bt1PAFunc5[0] = 32;
	initial_cfg.Bt1PAFunc6[0] = 32;
	//��ť1��������
	initial_cfg.Bt1AFunc1[0] = 1;
	initial_cfg.Bt1AFunc2[0] = 1;
	initial_cfg.Bt1AFunc3[0] = 0;
	initial_cfg.Bt1AFunc4[0] = 0;
	initial_cfg.Bt1AFunc5[0] = 2;
	initial_cfg.Bt1AFunc6[0] = 2;
	//����ִ�а�ť
	initial_cfg.MissionBt[0] = 12;
	//������ť
	initial_cfg.RTLBt[0] = 13;
	//��ȫ��ť
	initial_cfg.SafeBt[0] = 0;
	//��λ����
	initial_cfg.NeutralZone[0] = 5.0;
	//λ���ٶ���Ӧ����ϵ��
	initial_cfg.PosVelAlpha[0] = 1.6;
	//��̬��Ӧ����ϵ��
	initial_cfg.AttAlpha[0] = 1.5;
	//���ú���
	initial_cfg.RstWp0[0] = 0;
	
	MAV_PARAM_TYPE param_types[] = {
		//��ť1����ǰ���ܣ�ģʽ��ţ�
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		//��ť1��������
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		MAV_PARAM_TYPE_UINT8 ,
		//����ִ�а�ť
		MAV_PARAM_TYPE_UINT8 ,
		//������ť
		MAV_PARAM_TYPE_UINT8 ,
		//��ȫ��ť
		MAV_PARAM_TYPE_UINT8 ,
		//��λ����
		MAV_PARAM_TYPE_REAL32 ,
		//λ���ٶ���Ӧ����ϵ��
		MAV_PARAM_TYPE_REAL32 ,
		//��̬��Ӧ����ϵ��
		MAV_PARAM_TYPE_REAL32 ,
		//����ʹ��0�ź���
		MAV_PARAM_TYPE_UINT8 ,
	};
	SName param_names[] = {
		//��ť1����ǰ���ܣ�ģʽ��ţ�
		"MFunc_Bt1PAF1" ,
		"MFunc_Bt1PAF2" ,
		"MFunc_Bt1PAF3" ,
		"MFunc_Bt1PAF4" ,
		"MFunc_Bt1PAF5" ,
		"MFunc_Bt1PAF6" ,
		//��ť1��������
		"MFunc_Bt1AF1" ,
		"MFunc_Bt1AF2" ,
		"MFunc_Bt1AF3" ,
		"MFunc_Bt1AF4" ,
		"MFunc_Bt1AF5" ,
		"MFunc_Bt1AF6" ,
		//����ִ�а�ť
		"MFunc_MissionBt" ,
		//������ť
		"MFunc_RTLBt" ,
		//��ȫ��ť
		"MFunc_SafeBt" ,
		//��λ����
		"MFunc_NeutralZ" ,
		//λ���ٶ���Ӧ����ϵ��
		"MFunc_PVAlpha" ,
		//��̬��Ӧ����ϵ��
		"MFunc_AttAlpha" ,
		//����ʹ��0�ź���
		"MFunc_RstWp0"
	};
	ParamGroupRegister( "MFunc", 2, sizeof(ModeFuncCfg)/8, param_types, param_names, (uint64_t*)&initial_cfg );
	
	//ע�ᵱǰ������Ϣ
	CurrentWpInf initial_CurrentWpInf;
	initial_CurrentWpInf.CurrentWp[0] = 0;
	initial_CurrentWpInf.line_x = 0;
	initial_CurrentWpInf.line_y = 0;
	initial_CurrentWpInf.line_z = 0;
	initial_CurrentWpInf.line_fs = -1;
	ParamGroupRegister( "CurrentWp", 1, sizeof(CurrentWpInf)/8, 0, 0, (uint64_t*)&initial_CurrentWpInf );
	
	init_AuxFuncs();
	
	xTaskCreate( Modes_Server, "Modes", 4096, NULL, SysPriority_UserTask, NULL);
}