#pragma once

#include "Basic.hpp"

struct CommulinkConfig
{
	uint8_t sys_id[8];
	uint8_t comp_id[8];
};

/*������ʾ*/
	enum LEDSignal
	{
		LEDSignal_Start1 ,
		LEDSignal_Start2 ,
		
		LEDSignal_Continue1 ,
		LEDSignal_Success1 ,
		
		LEDSignal_Err1 ,
	};
	enum LEDMode
	{
		//�ر��Զ�ģʽ
		LEDMode_Manual ,
		
		//����ģʽ
		LEDMode_Normal1 ,
		LEDMode_Normal2 ,
		
		//����ģʽ
		LEDMode_Flying1 ,
		LEDMode_Flying2 ,
		
		//���ڴ���
		LEDMode_Processing1 ,
		LEDMode_Processing2 ,
	};
	void sendLedSignal( LEDSignal signal );
	void setLedMode( LEDMode mode );
	void setLedManualCtrl( float R, float G, float B, bool BuzzerOn, uint16_t BuzzerFreq );
/*������ʾ*/
	
/*ͨ�Ŷ˿�*/
	//�˿ڶ���
	typedef struct
	{
		//д�˿ں���
		uint16_t (*write)( const uint8_t* data, uint16_t length, double Write_waitTime, double Sync_waitTime );
		//������������
		bool (*lock)( double Sync_waitTime );
		void (*unlock)();
		//���˿ں���
		uint16_t (*read)( uint8_t* data , uint16_t length, double Rc_waitTime, double Sync_waitTime );
	}Port;

	//ע��˿�����Э��ͨ��
	bool CommuPortRegister( Port port );
	//��ָ���˿�������Ϣ����
	bool SetMsgRate( uint8_t port_index, uint16_t Msg, float RateHz, double TIMEOUT = -1 );
	//��ָ���˿ڷ�����Ϣ�б�
	void sendParamList();
	//��ȡ�˿�
	const Port* get_Port( uint8_t port );
	
	//�趨mavlinkģʽ
	bool set_mav_mode( uint16_t req_mav_mode, uint16_t req_mav_main_mode, uint16_t req_mav_sub_mode );
	bool get_mav_mode( uint16_t* req_mav_mode, uint16_t* req_mav_main_mode, uint16_t* req_mav_sub_mode );
	
	//��ȡ����id
	uint8_t get_CommulinkSysId();
	uint8_t get_CommulinkCompId();
/*ͨ�Ŷ˿�*/
	
/*RTK�˿�*/
	//�˿ڶ���
	typedef struct
	{
		bool ena;
		//д�˿ں���
		uint16_t (*write)( const uint8_t* data, uint16_t length, double Write_waitTime, double Sync_waitTime );
		//������������
		bool (*lock)( double Sync_waitTime );
		void (*unlock)();
	}RtkPort;
	
	//ע��Rtk�˿�
	int8_t RtkPortRegister( RtkPort port );
	//ʹ��ʧ��Rtk�˿�
	bool RtkPort_setEna( uint8_t port, bool ena );
	//��ȡ�˿�
	const RtkPort* get_RtkPort( uint8_t port );
	//��rtk�˿ڷ���ע������
	void inject_RtkPorts( const uint8_t data[], uint16_t length );
/*RTK�˿�*/
	
void init_Commulink();